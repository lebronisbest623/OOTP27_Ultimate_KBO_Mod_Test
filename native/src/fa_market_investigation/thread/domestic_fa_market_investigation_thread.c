#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../api/fa_market_investigation.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../fa_market_classification/api/fa_market_classification.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"

#define KBO_DOMESTIC_FA_INVESTIGATION_MAX 256

typedef struct KboDomesticFaInvestigationCandidate {
    KboFaMarketClassification row;
    int32_t value_score;
    int32_t overall;
    int32_t talent;
    int32_t ratings;
    int32_t career;
    uint32_t market_days;
    char blockers[160];
} KboDomesticFaInvestigationCandidate;

static volatile LONG g_kbo_domestic_fa_market_investigation_started = 0;

static int kbo_domestic_fa_market_investigation_enabled(void)
{
    return read_kbo_localappdata_flag_file("enable_kbo_domestic_fa_market_investigation.txt");
}

static uint32_t kbo_domestic_fa_date_gap_days(uint32_t older_yyyymmdd, uint32_t newer_yyyymmdd)
{
    if (older_yyyymmdd == 0u || newer_yyyymmdd == 0u || older_yyyymmdd > newer_yyyymmdd) {
        return 0u;
    }

    uint32_t older_serial = kbo_date_serial(
        older_yyyymmdd / 10000u,
        (older_yyyymmdd / 100u) % 100u,
        older_yyyymmdd % 100u);
    uint32_t newer_serial = kbo_date_serial(
        newer_yyyymmdd / 10000u,
        (newer_yyyymmdd / 100u) % 100u,
        newer_yyyymmdd % 100u);
    if (older_serial == 0u || newer_serial == 0u || older_serial > newer_serial) {
        return 0u;
    }
    return newer_serial - older_serial;
}

static uint32_t kbo_domestic_fa_history_date_from_reason(const char* reason)
{
    if (reason == NULL) {
        return 0u;
    }

    const char* marker = strstr(reason, "history=");
    if (marker == NULL) {
        return 0u;
    }
    marker += 8;

    char date_text[16] = {0};
    size_t pos = 0u;
    while (marker[pos] != '\0'
            && marker[pos] != ' '
            && marker[pos] != ';'
            && pos + 1u < sizeof(date_text)) {
        date_text[pos] = marker[pos];
        pos++;
    }
    date_text[pos] = '\0';

    uint32_t date = 0u;
    if (kbo_parse_yyyymmdd(date_text, &date)) {
        return date;
    }
    return 0u;
}

static int kbo_domestic_fa_case_is_market_relevant(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_DEFERRED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
        || strcmp(case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0;
}

static int kbo_domestic_fa_case_is_official_or_probable(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
        || strcmp(case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0;
}

static int kbo_domestic_fa_grade_is_quality(const char* grade)
{
    return grade != NULL
        && (strcmp(grade, "A") == 0
            || strcmp(grade, "B") == 0
            || strcmp(grade, "C") == 0);
}

static int kbo_domestic_fa_row_is_quality_candidate(
    const KboFaMarketClassification* row,
    int32_t value_score)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || !kbo_domestic_fa_case_is_market_relevant(row->case_label)) {
        return 0;
    }

    if (kbo_domestic_fa_grade_is_quality(row->grade)) {
        return 1;
    }
    if (row->fa_grade_salary >= 100000000) {
        return 1;
    }
    if (row->fa_demand >= 100000000) {
        return 1;
    }
    return value_score >= 55000;
}

static void kbo_domestic_fa_append_blocker(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0 || text == NULL || text[0] == '\0') {
        return;
    }
    size_t len = strlen(out);
    if (len > 0 && len + 1 < out_size) {
        out[len++] = '|';
        out[len] = '\0';
    }
    if (len + 1 < out_size) {
        snprintf(out + len, out_size - len, "%s", text);
    }
}

static void kbo_domestic_fa_describe_blockers(
    const KboFaMarketClassification* row,
    int32_t value_score,
    uint32_t market_days,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (row == NULL) {
        return;
    }

    if (!kbo_domestic_fa_case_is_official_or_probable(row->case_label)) {
        kbo_domestic_fa_append_blocker(out, out_size, "not_official_or_probable_fa");
    }
    if (row->fa_grade_team_changed_review) {
        kbo_domestic_fa_append_blocker(out, out_size, "team_changed_grade_review");
    }
    if (strcmp(row->fa_grade_flag, "SNAPSHOT_MISSING") == 0) {
        kbo_domestic_fa_append_blocker(out, out_size, "salary_snapshot_missing");
    }
    if (row->fa_demand >= 700000000) {
        kbo_domestic_fa_append_blocker(out, out_size, "very_high_demand");
    } else if (row->fa_demand >= 300000000) {
        kbo_domestic_fa_append_blocker(out, out_size, "high_demand");
    }
    if (strcmp(row->grade, "A") == 0 || strcmp(row->grade, "B") == 0) {
        kbo_domestic_fa_append_blocker(out, out_size, "compensation_burden");
    }
    if (row->age >= 36u) {
        kbo_domestic_fa_append_blocker(out, out_size, "age_36_plus");
    } else if (row->age >= 34u) {
        kbo_domestic_fa_append_blocker(out, out_size, "age_34_plus");
    }
    if (market_days >= 90u) {
        kbo_domestic_fa_append_blocker(out, out_size, "market_90d_plus");
    } else if (market_days >= 45u) {
        kbo_domestic_fa_append_blocker(out, out_size, "market_45d_plus");
    }
    if (value_score >= 85000 && out[0] == '\0') {
        kbo_domestic_fa_append_blocker(out, out_size, "high_quality_unexplained");
    }
    if (out[0] == '\0') {
        snprintf(out, out_size, "watch");
    }
}

static int kbo_domestic_fa_compare_candidates_desc(const void* lhs, const void* rhs)
{
    const KboDomesticFaInvestigationCandidate* a = (const KboDomesticFaInvestigationCandidate*)lhs;
    const KboDomesticFaInvestigationCandidate* b = (const KboDomesticFaInvestigationCandidate*)rhs;
    uint32_t grade_a = kbo_fa_market_display_grade_sort_rank(a->row.grade);
    uint32_t grade_b = kbo_fa_market_display_grade_sort_rank(b->row.grade);
    if (grade_a != grade_b) {
        return grade_a < grade_b ? -1 : 1;
    }
    if (a->value_score != b->value_score) {
        return a->value_score > b->value_score ? -1 : 1;
    }
    if (a->market_days != b->market_days) {
        return a->market_days > b->market_days ? -1 : 1;
    }
    if (a->row.age != b->row.age) {
        return a->row.age < b->row.age ? -1 : 1;
    }
    if (a->row.player_id == b->row.player_id) {
        return 0;
    }
    return a->row.player_id < b->row.player_id ? -1 : 1;
}

static void kbo_domestic_fa_csv_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

static void kbo_domestic_fa_csv_raw(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

static int kbo_domestic_fa_write_investigation_csv(
    const KboDomesticFaInvestigationCandidate* candidates,
    int candidate_count,
    const KboFaMarketScanSummary* summary,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (candidates == NULL || candidate_count < 0 || summary == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_file("domestic_fa_market_investigation.csv", path, sizeof(path))) {
        append_log_line("domestic FA market investigation: unable to resolve CSV path");
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("domestic FA market investigation: failed to open CSV path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    kbo_domestic_fa_csv_raw(
        file,
        "date,league_id,rank,player_id,name,age,case,grade,value_score,overall,talent,ratings,career,fa_demand,fa_grade_salary,fa_grade_overall_rank,fa_grade_team_rank,original_team_id,rights_team_id,market_days,fa_grade_flag,blockers,reason\r\n");

    char line[512] = {0};
    for (int i = 0; i < candidate_count; i++) {
        const KboDomesticFaInvestigationCandidate* candidate = &candidates[i];
        const KboFaMarketClassification* row = &candidate->row;
        int len = snprintf(
            line,
            sizeof(line),
            "%08u,%u,%d,%u,",
            summary->today_yyyymmdd,
            summary->league_id,
            i + 1,
            row->player_id);
        if (len <= 0 || len >= (int)sizeof(line)) {
            continue;
        }
        kbo_domestic_fa_csv_raw(file, line);
        kbo_domestic_fa_csv_text(file, row->player_name);
        len = snprintf(
            line,
            sizeof(line),
            ",%u,",
            (uint32_t)row->age);
        if (len <= 0 || len >= (int)sizeof(line)) {
            continue;
        }
        kbo_domestic_fa_csv_raw(file, line);
        kbo_domestic_fa_csv_text(file, row->case_label);
        kbo_domestic_fa_csv_raw(file, ",");
        kbo_domestic_fa_csv_text(file, row->grade);
        len = snprintf(
            line,
            sizeof(line),
            ",%d,%d,%d,%d,%d,%d,%d,%u,%u,%u,%u,%u,",
            candidate->value_score,
            candidate->overall,
            candidate->talent,
            candidate->ratings,
            candidate->career,
            row->fa_demand,
            row->fa_grade_salary,
            row->fa_grade_overall_rank,
            row->fa_grade_team_rank,
            row->original_team_id,
            row->rights_team_id,
            candidate->market_days);
        if (len <= 0 || len >= (int)sizeof(line)) {
            continue;
        }
        kbo_domestic_fa_csv_raw(file, line);
        kbo_domestic_fa_csv_text(file, row->fa_grade_flag);
        kbo_domestic_fa_csv_raw(file, ",");
        kbo_domestic_fa_csv_text(file, candidate->blockers);
        kbo_domestic_fa_csv_raw(file, ",");
        kbo_domestic_fa_csv_text(file, row->reason);
        kbo_domestic_fa_csv_raw(file, "\r\n");
    }

    CloseHandle(file);
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}

static int kbo_domestic_fa_run_investigation_once(uint32_t today, const char* source)
{
    KboFaMarketClassification* rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_CLASSIFICATION_MAX * sizeof(KboFaMarketClassification));
    KboDomesticFaInvestigationCandidate* candidates = (KboDomesticFaInvestigationCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_DOMESTIC_FA_INVESTIGATION_MAX * sizeof(KboDomesticFaInvestigationCandidate));
    if (rows == NULL || candidates == NULL) {
        if (rows != NULL) { HeapFree(GetProcessHeap(), 0, rows); }
        if (candidates != NULL) { HeapFree(GetProcessHeap(), 0, candidates); }
        append_log_line("domestic FA market investigation: allocation failed");
        return 0;
    }

    KboFaMarketScanSummary summary;
    memset(&summary, 0, sizeof(summary));
    int row_count = kbo_collect_fa_market_classifications(
        0u,
        rows,
        KBO_FA_MARKET_CLASSIFICATION_MAX,
        &summary,
        0,
        source);

    int domestic_rows = 0;
    int relevant_rows = 0;
    int quality_rows = 0;
    int grade_a_or_b = 0;
    int snapshot_missing = 0;
    int high_demand = 0;
    int long_market = 0;
    int unofficial_or_pending = 0;
    int candidate_count = 0;
    for (int i = 0; i < row_count; i++) {
        KboFaMarketClassification* row = &rows[i];
        if (row->nation_id != OOTP27_KBO_KOREA_NATION_ID || row->foreign_player) {
            continue;
        }
        domestic_rows++;
        if (!kbo_domestic_fa_case_is_market_relevant(row->case_label)) {
            continue;
        }
        relevant_rows++;

        int32_t value_score = 0;
        int32_t overall = 0;
        int32_t talent = 0;
        int32_t ratings = 0;
        int32_t career = 0;
        uint8_t* player = kbo_find_player_by_id(row->player_id, NULL, NULL);
        if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            value_score = kbo_foreign_waiver_value_score(player);
            overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
            talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
            ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
            career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
        }

        if (!kbo_domestic_fa_row_is_quality_candidate(row, value_score)) {
            continue;
        }
        quality_rows++;
        if (strcmp(row->grade, "A") == 0 || strcmp(row->grade, "B") == 0) {
            grade_a_or_b++;
        }
        if (strcmp(row->fa_grade_flag, "SNAPSHOT_MISSING") == 0) {
            snapshot_missing++;
        }
        if (row->fa_demand >= 300000000) {
            high_demand++;
        }
        if (!kbo_domestic_fa_case_is_official_or_probable(row->case_label)) {
            unofficial_or_pending++;
        }

        uint32_t history_date = kbo_domestic_fa_history_date_from_reason(row->reason);
        uint32_t market_days = kbo_domestic_fa_date_gap_days(history_date, today);
        if (market_days >= 45u) {
            long_market++;
        }

        if (candidate_count >= KBO_DOMESTIC_FA_INVESTIGATION_MAX) {
            continue;
        }

        KboDomesticFaInvestigationCandidate* candidate = &candidates[candidate_count++];
        candidate->row = *row;
        candidate->value_score = value_score;
        candidate->overall = overall;
        candidate->talent = talent;
        candidate->ratings = ratings;
        candidate->career = career;
        candidate->market_days = market_days;
        kbo_domestic_fa_describe_blockers(
            row,
            value_score,
            market_days,
            candidate->blockers,
            sizeof(candidate->blockers));
    }

    if (candidate_count > 1) {
        qsort(
            candidates,
            (size_t)candidate_count,
            sizeof(candidates[0]),
            kbo_domestic_fa_compare_candidates_desc);
    }

    char csv_path[MAX_PATH] = {0};
    int csv_written = kbo_domestic_fa_write_investigation_csv(
        candidates,
        candidate_count,
        &summary,
        csv_path,
        sizeof(csv_path));

    append_logf(
        "domestic FA market investigation summary date=%u league=%u rows=%d scanned=%d domestic=%d relevant=%d quality=%d grade_ab=%d high_demand=%d snapshot_missing=%d long_market=%d unofficial_or_pending=%d truncated=%d csv_written=%d csv=%s",
        today,
        summary.league_id,
        row_count,
        summary.scanned,
        domestic_rows,
        relevant_rows,
        quality_rows,
        grade_a_or_b,
        high_demand,
        snapshot_missing,
        long_market,
        unofficial_or_pending,
        summary.truncated,
        csv_written,
        csv_path);

    int top_count = candidate_count < 20 ? candidate_count : 20;
    for (int i = 0; i < top_count; i++) {
        const KboDomesticFaInvestigationCandidate* candidate = &candidates[i];
        const KboFaMarketClassification* row = &candidate->row;
        append_logf(
            "domestic FA orphan watch rank=%d player=%u name=\"%s\" age=%u case=%s grade=%s score=%d ovr=%d tal=%d rat=%d car=%d demand=%d grade_salary=%d overall_rank=%u team_rank=%u original_team=%u market_days=%u blockers=%s reason=%s",
            i + 1,
            row->player_id,
            row->player_name,
            (uint32_t)row->age,
            row->case_label,
            row->grade,
            candidate->value_score,
            candidate->overall,
            candidate->talent,
            candidate->ratings,
            candidate->career,
            row->fa_demand,
            row->fa_grade_salary,
            row->fa_grade_overall_rank,
            row->fa_grade_team_rank,
            row->original_team_id,
            candidate->market_days,
            candidate->blockers,
            row->reason);
    }

    HeapFree(GetProcessHeap(), 0, rows);
    HeapFree(GetProcessHeap(), 0, candidates);
    return candidate_count;
}

static DWORD WINAPI kbo_domestic_fa_market_investigation_thread(LPVOID parameter)
{
    (void)parameter;

    uint32_t last_scan_date = 0u;
    char last_save_path[MAX_PATH] = {0};
    int last_enabled = 0;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(5000)) {
            break;
        }

        int enabled = kbo_domestic_fa_market_investigation_enabled();
        if (!enabled) {
            last_enabled = 0;
            continue;
        }
        if (!last_enabled) {
            append_log_line("domestic FA market investigation enabled: observe-only mode active");
            last_scan_date = 0u;
            last_save_path[0] = '\0';
            last_enabled = 1;
        }

        uint32_t today = 0u;
        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_yyyymmdd(&today)
                || today == 0u
                || !kbo_get_current_save_path(save_path, sizeof(save_path))) {
            continue;
        }

        if (today == last_scan_date && strcmp(save_path, last_save_path) == 0) {
            continue;
        }

        kbo_domestic_fa_run_investigation_once(today, "domestic_fa_market_investigation");
        last_scan_date = today;
        snprintf(last_save_path, sizeof(last_save_path), "%s", save_path);
    }

    InterlockedExchange(&g_kbo_domestic_fa_market_investigation_started, 0);
    append_log_line("domestic FA market investigation thread stopped");
    return 0;
}

void start_kbo_domestic_fa_market_investigation_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_domestic_fa_market_investigation_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_domestic_fa_market_investigation_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "domestic FA market investigation");
    } else {
        InterlockedExchange(&g_kbo_domestic_fa_market_investigation_started, 0);
        append_logf("domestic FA market investigation thread failed error=%lu", GetLastError());
    }
}
