#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bootstrap/ootp_offsets.h"
#include "../core/core_atomic_file.h"
#include "../core/core_flags/flags_api.h"
#include "../core/core_league_context_parts/league_context_lookup.h"
#include "../core/core_log.h"
#include "../core/core_save_paths.h"
#include "../fa_compensation/fa_compensation_history.h"
#include "../foreign/foreign_waiver_date.h"
#include "../foreign/foreign_waiver_player_eval.h"
#include "../foreign/foreign_waiver_policy.h"
#include "../foreign/injury/foreign_injury_labels.h"
#include "../military_service/military_service_team_policy.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/team_lookup.h"
#include "../team/team_roster_arrays.h"
#include "fa_requalification.h"
#include <stdint.h>

/* ---- KBO FA requalification control ---- */

#ifndef KBO_FA_REQUALIFICATION_TYPES_DEFINED
#define KBO_FA_REQUALIFICATION_TYPES_DEFINED

#define KBO_FA_REQUALIFICATION_YEARS 4
#define KBO_FA_REQUALIFICATION_MAX 4096

typedef struct KboFaRequalificationRecord {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t last_fa_year;
    uint32_t fa_count;
} KboFaRequalificationRecord;

#endif

static LONG g_kbo_fa_requalification_thread_started = 0;
static LONG g_kbo_fa_requalification_no_date_log_count = 0;
static LONG g_kbo_fa_requalification_no_records_log_count = 0;
static LONG g_kbo_fa_requalification_skip_log_count = 0;
static LONG g_kbo_fa_requalification_hook_skip_log_count = 0;
static volatile LONG g_kbo_fa_requalification_records_lock = 0;
static uint32_t g_kbo_fa_requalification_last_no_records_date = 0;

static int get_kbo_fa_requalification_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_requalification.csv", out, out_size);
}

static void kbo_lock_fa_requalification_records(void)
{
    while (InterlockedCompareExchange(&g_kbo_fa_requalification_records_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_fa_requalification_records(void)
{
    InterlockedExchange(&g_kbo_fa_requalification_records_lock, 0);
}


static int kbo_fa_parse_u32_csv_field(const char** cursor, uint32_t* out_value)
{
    if (cursor == NULL || *cursor == NULL || out_value == NULL) {
        return 0;
    }

    const char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }
    if (*p == '"') {
        p++;
    }

    char* tail = NULL;
    unsigned long value = strtoul(p, &tail, 10);
    if (tail == p || value > 0xfffffffful) {
        return 0;
    }

    while (*tail != '\0' && *tail != ',' && *tail != '\n') {
        tail++;
    }
    if (*tail == ',') {
        tail++;
    }

    *cursor = tail;
    *out_value = (uint32_t)value;
    return 1;
}

static void kbo_ensure_fa_requalification_template(void)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_fa_requalification_path(path, sizeof(path))) {
        return;
    }

    DWORD attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    const char* header =
        "player_id,original_team_id,last_fa_year,fa_count\r\n"
        "# KBO FA requalification: after any FA signing, restore team control until last_fa_year + 4.\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    CloseHandle(file);
    append_logf("KBO FA requalification template created path=%s", path);
}

int kbo_load_fa_requalification_records(KboFaRequalificationRecord* records, int max_records)
{
    if (records == NULL || max_records <= 0) {
        return 0;
    }
    memset(records, 0, (SIZE_T)max_records * sizeof(records[0]));
    kbo_ensure_fa_requalification_template();

    char path[MAX_PATH] = {0};
    if (!get_kbo_fa_requalification_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    if (!ReadFile(file, buffer, size, &read, NULL)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    buffer[read] = '\0';

    int count = 0;
    const char* cursor = buffer;
    while (*cursor != '\0' && count < max_records) {
        const char* next = strchr(cursor, '\n');
        size_t len = next != NULL ? (size_t)(next - cursor) : strlen(cursor);
        if (len > 0 && len < 256) {
            char line[256] = {0};
            memcpy(line, cursor, len);
            const char* p = line;
            while (*p == ' ' || *p == '\t' || *p == '\r') {
                p++;
            }
            if (*p != '\0' && *p != '#' && *p != ';' && (*p < 'A' || *p > 'z')) {
                uint32_t player_id = 0;
                uint32_t original_team_id = 0;
                uint32_t last_fa_year = 0;
                uint32_t fa_count = 0;
                if (kbo_fa_parse_u32_csv_field(&p, &player_id)
                        && kbo_fa_parse_u32_csv_field(&p, &original_team_id)
                        && kbo_fa_parse_u32_csv_field(&p, &last_fa_year)
                        && kbo_fa_parse_u32_csv_field(&p, &fa_count)
                        && player_id != 0u
                        && original_team_id != 0u
                        && last_fa_year >= 1982u
                        && last_fa_year <= 2200u
                        && fa_count >= 1u) {
                    records[count++] = (KboFaRequalificationRecord){
                        player_id,
                        original_team_id,
                        last_fa_year,
                        fa_count
                    };
                }
            }
        }
        if (next == NULL || *next == '\0') {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return count;
}

static int kbo_write_fa_requalification_records(const KboFaRequalificationRecord* records, int count)
{
    if (records == NULL || count < 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_fa_requalification_path(path, sizeof(path))) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA requalification write failed path=%s err=%lu", path, GetLastError());
        return 0;
    }

    char line[256] = {0};
    DWORD written = 0;
    const char* header =
        "player_id,original_team_id,last_fa_year,fa_count\r\n"
        "# KBO FA requalification: after any FA signing, restore team control until last_fa_year + 4.\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < count; i++) {
        if (records[i].player_id == 0u || records[i].original_team_id == 0u || records[i].last_fa_year == 0u) {
            continue;
        }
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u\r\n",
            records[i].player_id,
            records[i].original_team_id,
            records[i].last_fa_year,
            records[i].fa_count);
        if (len > 0) {
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
    }

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        append_logf("KBO FA requalification write: atomic commit failed path=%s", path);
        return 0;
    }
    return 1;
}

static int kbo_record_fa_requalification_signing(uint32_t player_id, uint32_t team_id, uint32_t signing_year, const char* source)
{
    if (player_id == 0u || team_id == 0u || signing_year < 1982u || signing_year > 2200u) {
        return 0;
    }

    kbo_lock_fa_requalification_records();
    KboFaRequalificationRecord records[KBO_FA_REQUALIFICATION_MAX];
    int count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (records[i].player_id == player_id) {
            found = i;
            break;
        }
    }

    if (found >= 0) {
        if (records[found].original_team_id == team_id && records[found].last_fa_year == signing_year) {
            append_logf(
                "KBO FA requalification signing already recorded source=%s player=%u team=%u signing_year=%u fa_count=%u",
                source != NULL ? source : "",
                player_id,
                team_id,
                signing_year,
                records[found].fa_count);
            kbo_unlock_fa_requalification_records();
            return 1;
        }
        records[found].original_team_id = team_id;
        records[found].last_fa_year = signing_year;
        records[found].fa_count = records[found].fa_count + 1u;
        if (records[found].fa_count == 0u) {
            records[found].fa_count = 1u;
        }
    } else {
        if (count >= KBO_FA_REQUALIFICATION_MAX) {
            append_logf("KBO FA requalification signing record dropped source=%s player=%u team=%u reason=max_records", source != NULL ? source : "", player_id, team_id);
            kbo_unlock_fa_requalification_records();
            return 0;
        }
        records[count++] = (KboFaRequalificationRecord){
            player_id,
            team_id,
            signing_year,
            1u
        };
    }

    int ok = kbo_write_fa_requalification_records(records, count);
    append_logf(
        "KBO FA requalification signing recorded source=%s player=%u team=%u signing_year=%u eligible_year=%u records=%d ok=%d",
        source != NULL ? source : "",
        player_id,
        team_id,
        signing_year,
        signing_year + KBO_FA_REQUALIFICATION_YEARS,
        count,
        ok);
    kbo_unlock_fa_requalification_records();
    return ok;
}

uint8_t* kbo_find_fa_requalification_player_by_id(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}

static int kbo_fa_requalification_team_ptr_is_kbo(
    uintptr_t team_ptr,
    uint32_t* out_team_id,
    uint32_t* out_league_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (out_team_id != NULL) {
        *out_team_id = team_id;
    }
    if (out_league_id != NULL) {
        *out_league_id = league_id;
    }
    if (team_id == 0u || team_id > 100000u || league_id == 0u || league_id > 100000u) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    return kbo_league_id == 0u || league_id == kbo_league_id;
}

__declspec(noinline) int ootp_kbo_fa_signing_branch_wrapper(uintptr_t player_ptr, uintptr_t team_ptr)
{
    if (!kbo_fix_enabled()) {
        return 1;
    }
    if (!kbo_player_pointer_plausible(player_ptr)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=bad_player player_ptr=%p team_ptr=%p", (void*)player_ptr, (void*)team_ptr);
        }
        return 1;
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u || player_id > 1000000u) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=bad_player_id player=%u team=%u league=%u", player_id, team_id, league_id);
        }
        return 1;
    }

    int is_kbo_team = kbo_fa_requalification_team_ptr_is_kbo(team_ptr, &team_id, &league_id);

    if (kbo_team_id_is_military_service_team(team_id)) {
        static volatile LONG military_fa_signing_block_log_count = 0;
        LONG slot = InterlockedIncrement(&military_fa_signing_block_log_count);
        if (slot <= 200) {
            append_logf(
                "military service team FA signing blocked player=%u team=%u league=%u",
                player_id,
                team_id,
                league_id);
        }
        return 0;
    }

    if (!is_kbo_team) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=non_kbo_team player=%u team_ptr=%p team=%u league=%u", player_id, (void*)team_ptr, team_id, league_id);
        }
        return 1;
    }

    if (kbo_custom_foreign_policy_enabled()
            && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = 0u;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_final_signing(
            team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            uint32_t today = 0u;
            if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
                kbo_get_current_yyyymmdd(&today);
            }
            kbo_record_recent_custom_foreign_policy_block(player_id, team_id, today);
            static volatile LONG final_block_log_count = 0;
            LONG slot = InterlockedIncrement(&final_block_log_count);
            if (slot <= 200) {
                append_logf(
                    "custom foreign policy FA signing blocked player=%u team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    player_id,
                    team_id,
                    effective_before,
                    effective_after,
                    effective_limit,
                    kbo_foreign_injury_slot_label(slot_type),
                    injured_player_id,
                    today);
            }
            return 0;
        }
    }

    return 1;
}

__declspec(noinline) void ootp_kbo_fa_signing_success_post_wrapper(uintptr_t player_ptr, uintptr_t team_ptr)
{
    if (!kbo_fix_enabled() || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    if (!kbo_fa_requalification_team_ptr_is_kbo(team_ptr, &team_id, &league_id)) {
        return;
    }
    if (kbo_team_id_is_military_service_team(team_id)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET)
        : 0u;

    static volatile LONG post_log_count = 0;
    LONG slot = InterlockedIncrement(&post_log_count);
    if (slot <= 120) {
        append_logf(
            "KBO FA signing success post player=%u team=%u league=%u current_team=%u active_team=%u original_team=%u",
            player_id,
            team_id,
            league_id,
            current_team_id,
            active_team_id,
            original_team_id);
    }

    kbo_record_fa_compensation_signing(player_ptr, team_id, league_id, "fa_signing_success_post");

    if (!read_kbo_localappdata_flag_file("enable_fa_requalification.txt")) {
        return;
    }

    uint32_t signing_year = kbo_find_league_year_from_id(league_id);
    if (signing_year < 1982u || signing_year > 2200u) {
        uint32_t today = 0u;
        if (kbo_get_current_yyyymmdd(&today)) {
            signing_year = today / 10000u;
        }
    }
    if (signing_year < 1982u || signing_year > 2200u) {
        LONG skip_slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (skip_slot <= 20) {
            append_logf("KBO FA signing success post skipped reason=no_signing_year player=%u team=%u league=%u", player_id, team_id, league_id);
        }
        return;
    }

    kbo_record_fa_requalification_signing(player_id, team_id, signing_year, "fa_signing_success_post");
}

static int kbo_restore_fa_requalification_team_control(
    const KboFaRequalificationRecord* rec,
    uint32_t current_year,
    const char* source)
{
    if (rec == NULL || rec->player_id == 0u || rec->original_team_id == 0u
            || rec->last_fa_year < 1982u) {
        return 0;
    }
    if (current_year >= rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=already_eligible current_year=%u last_fa_year=%u eligible_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year,
                rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS);
        }
        return 0;
    }

    uint8_t* player = kbo_find_fa_requalification_player_by_id(rec->player_id);
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->original_team_id, 1);
    if (player == NULL) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=player_not_found current_year=%u last_fa_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year);
        }
        return 0;
    }
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=team_not_found current_year=%u last_fa_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year);
        }
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t old_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t old_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t old_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int changed = 0;

    if (old_current_team != rec->original_team_id) {
        if (old_current_team != 0u) {
            uint8_t* old_team = find_kbo_team_by_numeric_id_any_league(old_current_team, 1);
            if (old_team != NULL) {
                kbo_remove_player_id_from_known_team_roster_arrays(old_team, rec->player_id);
            }
        }
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = rec->original_team_id;
        changed = 1;
    }
    if (old_active_team != rec->original_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = rec->original_team_id;
        changed = 1;
    }
    if (league_id != 0u && old_league != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (league_id != 0u && *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    kbo_add_player_id_to_team_assignment_arrays(team, rec->player_id);

    if (changed) {
        append_logf(
            "KBO FA requalification restored team control source=%s player=%u team=%u year=%u last_fa_year=%u eligible_year=%u old_team=%u old_active=%u old_league=%u league=%u",
            source != NULL ? source : "",
            rec->player_id,
            rec->original_team_id,
            current_year,
            rec->last_fa_year,
            rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS,
            old_current_team,
            old_active_team,
            old_league,
            league_id);
    } else {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification checked source=%s player=%u team=%u reason=already_controlled current_year=%u last_fa_year=%u eligible_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year,
                rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS);
        }
    }
    return changed;
}

static void kbo_run_fa_requalification_once(const char* source)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_no_date_log_count);
        if (slot <= 20 || (slot % 60) == 0) {
            append_logf(
                "KBO FA requalification skipped source=%s reason=current_date_unavailable count=%ld",
                source != NULL ? source : "",
                slot);
        }
        return;
    }
    uint32_t current_year = 0u;
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id != 0u) {
        current_year = kbo_find_league_year_from_id(kbo_league_id);
    }
    if (current_year < 1982u || current_year > 2200u) {
        current_year = today / 10000u;
    }
    if (current_year < 1982u || current_year > 2200u) {
        append_logf(
            "KBO FA requalification skipped source=%s reason=invalid_current_year today=%u current_year=%u",
            source != NULL ? source : "",
            today,
            current_year);
        return;
    }

    KboFaRequalificationRecord records[KBO_FA_REQUALIFICATION_MAX];
    kbo_lock_fa_requalification_records();
    int count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    kbo_unlock_fa_requalification_records();
    if (count <= 0) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_no_records_log_count);
        if (g_kbo_fa_requalification_last_no_records_date != today || slot <= 5 || (slot % 60) == 0) {
            char path[MAX_PATH] = {0};
            get_kbo_fa_requalification_path(path, sizeof(path));
            append_logf(
                "KBO FA requalification pass source=%s records=0 restored=0 today=%u path=%s",
                source != NULL ? source : "",
                today,
                path);
            g_kbo_fa_requalification_last_no_records_date = today;
        }
        return;
    }
    int restored = 0;
    for (int i = 0; i < count; i++) {
        restored += kbo_restore_fa_requalification_team_control(&records[i], current_year, source);
    }
    append_logf("KBO FA requalification pass source=%s records=%d restored=%d today=%u", source != NULL ? source : "", count, restored, today);
}

static DWORD WINAPI kbo_fa_requalification_thread(LPVOID parameter)
{
    (void)parameter;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(5000)) {
            break;
        }
        kbo_run_fa_requalification_once("fa_requalification_monitor");
    }
    InterlockedExchange(&g_kbo_fa_requalification_thread_started, 0);
    append_log_line("KBO FA requalification monitor thread stopped");
    return 0;
}

void start_kbo_fa_requalification_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_fa_requalification_thread_started, 1, 0) != 0) {
        return;
    }
    HANDLE thread = CreateThread(NULL, 0, kbo_fa_requalification_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("KBO FA requalification monitor thread started");
    } else {
        InterlockedExchange(&g_kbo_fa_requalification_thread_started, 0);
        append_log_line("KBO FA requalification monitor thread failed to start");
    }
}


