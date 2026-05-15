#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_compensation_cash_transfer.h"

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/csv/core_csv.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"

#define KBO_FA_COMPENSATION_CASH_TRANSFER_FILE "fa_compensation_cash_transfers.csv"
#define KBO_FA_COMP_TEAM_FINANCIALS_BLOCK_OFFSET 0x2510u
#define KBO_FA_COMP_TEAM_FINANCIALS_BUDGET_OFFSET 0x78u
#define KBO_FA_COMP_TEAM_FINANCIALS_SCOUTING_BUDGET_OFFSET 0x90u
#define KBO_FA_COMP_TEAM_FINANCIALS_DEVELOPMENT_BUDGET_OFFSET 0x94u
#define KBO_FA_COMP_TEAM_FINANCIALS_DRAFT_BUDGET_OFFSET 0x98u
#define KBO_FA_COMP_TEAM_FINANCIALS_DRAFT_EXPENSES_OFFSET 0x9cu
#define KBO_FA_COMP_TEAM_FINANCIALS_CASH_OFFSET 0xc0u
#define KBO_FA_COMP_TEAM_FINANCIALS_READABLE_BYTES (KBO_FA_COMP_TEAM_FINANCIALS_CASH_OFFSET + sizeof(int32_t))
#define KBO_FA_COMP_FINANCIAL_FIELD_ABS_LIMIT 2000000000

static int kbo_fa_comp_cash_transfer_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(KBO_FA_COMPENSATION_CASH_TRANSFER_FILE, out, out_size);
}

static int kbo_fa_comp_abs_i32_plausible(int32_t value)
{
    return value > -KBO_FA_COMP_FINANCIAL_FIELD_ABS_LIMIT
        && value < KBO_FA_COMP_FINANCIAL_FIELD_ABS_LIMIT;
}

static int kbo_fa_comp_financial_block_plausible(uint8_t* financials)
{
    if (financials == NULL
            || !memory_range_readable(financials, KBO_FA_COMP_TEAM_FINANCIALS_READABLE_BYTES)) {
        return 0;
    }

    int32_t budget = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_BUDGET_OFFSET);
    int32_t scouting_budget = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_SCOUTING_BUDGET_OFFSET);
    int32_t development_budget = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_DEVELOPMENT_BUDGET_OFFSET);
    int32_t draft_budget = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_DRAFT_BUDGET_OFFSET);
    int32_t draft_expenses = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_DRAFT_EXPENSES_OFFSET);
    int32_t cash = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_CASH_OFFSET);

    return kbo_fa_comp_abs_i32_plausible(budget)
        && kbo_fa_comp_abs_i32_plausible(scouting_budget)
        && kbo_fa_comp_abs_i32_plausible(development_budget)
        && kbo_fa_comp_abs_i32_plausible(draft_budget)
        && kbo_fa_comp_abs_i32_plausible(draft_expenses)
        && kbo_fa_comp_abs_i32_plausible(cash);
}

static int32_t* kbo_fa_comp_team_cash_ptr(uint32_t team_id)
{
    if (team_id == 0u) {
        return NULL;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL
            || !memory_range_readable(
                team + KBO_FA_COMP_TEAM_FINANCIALS_BLOCK_OFFSET,
                KBO_FA_COMP_TEAM_FINANCIALS_READABLE_BYTES)) {
        return NULL;
    }
    uint8_t* financials = team + KBO_FA_COMP_TEAM_FINANCIALS_BLOCK_OFFSET;
    if (!kbo_fa_comp_financial_block_plausible(financials)) {
        return NULL;
    }
    return (int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_CASH_OFFSET);
}

static int kbo_fa_comp_cash_transfer_already_applied(
    uint32_t season,
    uint32_t player_id,
    uint32_t signing_team_id,
    uint32_t original_team_id,
    const char* action)
{
    if (season == 0u || player_id == 0u || signing_team_id == 0u || original_team_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_comp_cash_transfer_path(path, sizeof(path))) {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int found = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[13][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 13);
        if (field_count < 6 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }
        uint32_t row_season = (uint32_t)strtoul(fields[0], NULL, 10);
        uint32_t row_player_id = (uint32_t)strtoul(fields[1], NULL, 10);
        uint32_t row_signing_team_id = (uint32_t)strtoul(fields[2], NULL, 10);
        uint32_t row_original_team_id = (uint32_t)strtoul(fields[3], NULL, 10);
        if (row_season == season
                && row_player_id == player_id
                && row_signing_team_id == signing_team_id
                && row_original_team_id == original_team_id
                && (action == NULL || action[0] == '\0' || strcmp(fields[5], action) == 0)) {
            found = 1;
            break;
        }
    }

    kbo_csv_reader_close(reader);
    return found;
}

static void kbo_fa_comp_copy_csv_token(const char* in, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (in == NULL) {
        return;
    }
    size_t used = 0u;
    for (const char* p = in; *p != '\0' && used + 1u < out_size; p++) {
        char ch = *p;
        if (ch == ',' || ch == '\r' || ch == '\n') {
            ch = ' ';
        }
        out[used++] = ch;
    }
    out[used] = '\0';
}

static int kbo_fa_comp_append_cash_transfer_ledger(
    const KboFaCompensationRecord* rec,
    uint32_t amount,
    uint32_t applied_yyyymmdd,
    const char* action,
    const char* source,
    int32_t signing_old_cash,
    int32_t signing_new_cash,
    int32_t original_old_cash,
    int32_t original_new_cash)
{
    char path[MAX_PATH] = {0};
    if (!kbo_fa_comp_cash_transfer_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "KBO FA compensation cash transfer ledger open failed player=%u gle=%lu path=%s",
            rec != NULL ? rec->player_id : 0u,
            (unsigned long)GetLastError(),
            path);
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    DWORD written = 0u;
    if (size == 0u && high == 0u) {
        const char* header =
            "season,player_id,signing_team_id,original_team_id,amount,action,applied_date,"
            "signing_old_cash,signing_new_cash,original_old_cash,original_new_cash,source,player_name\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char safe_action[64] = {0};
    char safe_source[96] = {0};
    char safe_name[96] = {0};
    kbo_fa_comp_copy_csv_token(action != NULL ? action : "", safe_action, sizeof(safe_action));
    kbo_fa_comp_copy_csv_token(source != NULL ? source : "", safe_source, sizeof(safe_source));
    kbo_fa_comp_copy_csv_token(rec != NULL ? rec->player_name : "", safe_name, sizeof(safe_name));

    char line[640] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%u,%u,%u,%u,%u,%s,%u,%d,%d,%d,%d,%s,%s\r\n",
        rec->season,
        rec->player_id,
        rec->signing_team_id,
        rec->original_team_id,
        amount,
        safe_action,
        applied_yyyymmdd,
        signing_old_cash,
        signing_new_cash,
        original_old_cash,
        original_new_cash,
        safe_source,
        safe_name);
    int ok = len > 0 && len < (int)sizeof(line)
        && WriteFile(file, line, (DWORD)len, &written, NULL)
        && written == (DWORD)len;
    CloseHandle(file);
    return ok;
}

static int32_t kbo_fa_comp_add_cash_clamped(int32_t old_cash, int64_t delta)
{
    int64_t next = (int64_t)old_cash + delta;
    if (next < (int64_t)INT32_MIN) {
        return INT32_MIN;
    }
    if (next > (int64_t)INT32_MAX) {
        return INT32_MAX;
    }
    return (int32_t)next;
}

int kbo_apply_fa_compensation_cash_transfer(
    const KboFaCompensationRecord* rec,
    uint32_t amount,
    uint32_t applied_yyyymmdd,
    const char* action,
    const char* source)
{
    if (rec == NULL
            || rec->player_id == 0u
            || rec->season == 0u
            || rec->signing_team_id == 0u
            || rec->original_team_id == 0u
            || rec->signing_team_id == rec->original_team_id
            || amount == 0u
            || applied_yyyymmdd == 0u) {
        return 0;
    }
    if (kbo_fa_comp_cash_transfer_already_applied(
            rec->season,
            rec->player_id,
            rec->signing_team_id,
            rec->original_team_id,
            action)) {
        kbo_log_runtimef(
            "KBO FA compensation cash transfer skipped source=%s reason=already_applied player=%u season=%u action=%s",
            source != NULL ? source : "",
            rec->player_id,
            rec->season,
            action != NULL ? action : "");
        return 0;
    }

    int32_t* signing_cash = kbo_fa_comp_team_cash_ptr(rec->signing_team_id);
    int32_t* original_cash = kbo_fa_comp_team_cash_ptr(rec->original_team_id);
    if (signing_cash == NULL || original_cash == NULL) {
        kbo_log_runtimef(
            "KBO FA compensation cash transfer skipped source=%s reason=team_cash_unavailable player=%u signing_team=%u original_team=%u amount=%u",
            source != NULL ? source : "",
            rec->player_id,
            rec->signing_team_id,
            rec->original_team_id,
            amount);
        return 0;
    }

    int32_t signing_old_cash = *signing_cash;
    int32_t original_old_cash = *original_cash;
    int32_t signing_new_cash = kbo_fa_comp_add_cash_clamped(signing_old_cash, -(int64_t)amount);
    int32_t original_new_cash = kbo_fa_comp_add_cash_clamped(original_old_cash, (int64_t)amount);
    *signing_cash = signing_new_cash;
    *original_cash = original_new_cash;

    if (!kbo_fa_comp_append_cash_transfer_ledger(
            rec,
            amount,
            applied_yyyymmdd,
            action,
            source,
            signing_old_cash,
            signing_new_cash,
            original_old_cash,
            original_new_cash)) {
        kbo_log_runtimef(
            "KBO FA compensation cash transfer ledger failed source=%s player=%u amount=%u action=%s",
            source != NULL ? source : "",
            rec->player_id,
            amount,
            action != NULL ? action : "");
    }

    kbo_log_runtimef(
        "KBO FA compensation cash transfer applied source=%s player=%u action=%s amount=%u signing_team=%u cash=%d->%d original_team=%u cash=%d->%d date=%u",
        source != NULL ? source : "",
        rec->player_id,
        action != NULL ? action : "",
        amount,
        rec->signing_team_id,
        signing_old_cash,
        signing_new_cash,
        rec->original_team_id,
        original_old_cash,
        original_new_cash,
        applied_yyyymmdd);
    return 1;
}
