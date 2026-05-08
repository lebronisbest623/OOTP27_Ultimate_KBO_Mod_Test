#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_atomic_file.h"
#include "../../core/core_current_date.h"
#include "../../core/core_flags/flags_api.h"
#include "../../core/core_league_context_parts/league_context_lookup.h"
#include "../../core/core_live_news.h"
#include "../../core/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/team_lookup.h"
#include "../../team/team_name_cache.h"
#include "../foreign_csv_parse.h"
#include "../foreign_waiver_date.h"
#include "../foreign_waiver_player_eval.h"
#include "../foreign_waiver_policy.h"
#include "foreign_injury_paths.h"

#ifndef KBO_FOREIGN_INJURY_SLOT_REGULAR
#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#endif
#ifndef KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2
#endif

/* Foreign injury replacement slot lifecycle. Included from native/src/foreign_waiver_ai.inc. */

#define KBO_FOREIGN_INJURY_REPLACEMENT_MIN_DAYS 42
#define KBO_FOREIGN_INJURY_REPLACEMENT_MAX      256
#define KBO_FOREIGN_INJURY_STATUS_OPEN          1
#define KBO_FOREIGN_INJURY_STATUS_ACTIVE        2
#define KBO_FOREIGN_INJURY_STATUS_PENDING       3
#define KBO_FOREIGN_INJURY_STATUS_CLOSED        4

typedef struct KboForeignInjuryReplacement {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t injured_player_id;
    uint32_t replacement_player_id;
    uint32_t opened_on_yyyymmdd;
    uint32_t expected_end_yyyymmdd;
    uint8_t  slot_type;
    uint8_t  status;
    uint8_t  converted;
} KboForeignInjuryReplacement;

KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX] = {{0}};
int  g_kbo_foreign_injury_replacement_count = 0;
static LONG g_kbo_foreign_injury_replacement_lock = 0;
static char g_kbo_foreign_injury_replacement_loaded_path[MAX_PATH] = {0};

static int kbo_persist_foreign_injury_replacements_locked(void);
static int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed);

int kbo_foreign_injury_replacement_enabled(void)
{
    return kbo_fix_enabled() && !read_kbo_localappdata_flag_file("disable_foreign_injury_replacement.txt");
}


/* Foreign injury replacement labels, slot helpers, and lock helpers. Included from native/KBOFix.c. */

const char* kbo_foreign_injury_slot_label(uint8_t slot_type)
{
    return slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular";
}

const char* kbo_foreign_injury_status_label(uint8_t status)
{
    switch (status) {
    case KBO_FOREIGN_INJURY_STATUS_OPEN:    return "Open";
    case KBO_FOREIGN_INJURY_STATUS_ACTIVE:  return "Active";
    case KBO_FOREIGN_INJURY_STATUS_PENDING: return "Decision due";
    case KBO_FOREIGN_INJURY_STATUS_CLOSED:  return "Closed";
    default:                                return "Unknown";
    }
}

int kbo_foreign_injury_status_uses_slot(uint8_t status)
{
    return status == KBO_FOREIGN_INJURY_STATUS_OPEN || status == KBO_FOREIGN_INJURY_STATUS_ACTIVE;
}

uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player)
{
    return kbo_player_is_asian_quota_candidate(player)
        ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
        : KBO_FOREIGN_INJURY_SLOT_REGULAR;
}

void kbo_lock_foreign_injury_replacements(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_injury_replacement_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_unlock_foreign_injury_replacements(void)
{
    InterlockedExchange(&g_kbo_foreign_injury_replacement_lock, 0);
}

/* Foreign injury replacement CSV loading. Included from native/KBOFix.c. */

static int kbo_load_foreign_injury_replacements_locked(const char* path)
{
    g_kbo_foreign_injury_replacement_count = 0;
    if (path == NULL || path[0] == '\0') {
        g_kbo_foreign_injury_replacement_loaded_path[0] = '\0';
        return 0;
    }
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 262144u) {
        CloseHandle(file);
        return 0;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int loaded = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0' && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[256] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            const char* p = line;
            uint32_t team_id = 0u;
            uint32_t league_id = 0u;
            uint32_t injured_player_id = 0u;
            uint32_t replacement_player_id = 0u;
            uint32_t opened_on = 0u;
            uint32_t expected_end = 0u;
            uint32_t slot_type = 0u;
            uint32_t status = 0u;
            uint32_t converted = 0u;
            if (line[0] >= '0' && line[0] <= '9'
                    && parse_u32_from_csv_field(&p, &team_id)
                    && parse_u32_from_csv_field(&p, &league_id)
                    && parse_u32_from_csv_field(&p, &injured_player_id)
                    && parse_u32_from_csv_field(&p, &replacement_player_id)
                    && parse_u32_from_csv_field(&p, &opened_on)
                    && parse_u32_from_csv_field(&p, &expected_end)
                    && parse_u32_from_csv_field(&p, &slot_type)
                    && parse_u32_from_csv_field(&p, &status)
                    && parse_u32_from_csv_field(&p, &converted)
                    && team_id != 0u
                    && injured_player_id != 0u
                    && slot_type >= KBO_FOREIGN_INJURY_SLOT_REGULAR
                    && slot_type <= KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                    && status >= KBO_FOREIGN_INJURY_STATUS_OPEN
                    && status <= KBO_FOREIGN_INJURY_STATUS_CLOSED) {
                KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++];
                rec->team_id = team_id;
                rec->league_id = league_id;
                rec->injured_player_id = injured_player_id;
                rec->replacement_player_id = replacement_player_id;
                rec->opened_on_yyyymmdd = opened_on;
                rec->expected_end_yyyymmdd = expected_end;
                rec->slot_type = (uint8_t)slot_type;
                rec->status = (uint8_t)status;
                rec->converted = converted ? 1u : 0u;
                loaded++;
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    append_logf("foreign injury replacement: loaded=%d path=%s", loaded, path);
    return loaded;
}

/* Foreign injury replacement seed import. Included from native/KBOFix.c. */

static int kbo_parse_foreign_injury_replacement_seed_line(
    const char* line,
    uint32_t today,
    KboForeignInjuryReplacement* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    const char* p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';') {
        return 0;
    }

    uint32_t values[9] = {0};
    int count = 0;
    while (count < 9) {
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }
        if (*p < '0' || *p > '9') {
            break;
        }
        if (!parse_u32_from_csv_field(&p, &values[count])) {
            break;
        }
        count++;
        while (*p != '\0' && *p != ',' && *p != '\r' && *p != '\n') {
            p++;
        }
        if (*p == '\0' || *p == '\r' || *p == '\n') {
            break;
        }
    }

    if (count < 3) {
        return 0;
    }

    if (count >= 9) {
        out->team_id = values[0];
        out->league_id = values[1];
        out->injured_player_id = values[2];
        out->replacement_player_id = values[3];
        out->opened_on_yyyymmdd = values[4];
        out->expected_end_yyyymmdd = values[5];
        out->slot_type = (uint8_t)values[6];
        out->status = (uint8_t)values[7];
        out->converted = values[8] ? 1u : 0u;
    } else {
        out->team_id = values[0];
        out->injured_player_id = values[1];
        out->replacement_player_id = values[2];
        out->slot_type = (count >= 4) ? (uint8_t)values[3] : 0u;
        out->status = (count >= 5) ? (uint8_t)values[4] : 0u;
        out->opened_on_yyyymmdd = today;
    }

    if (out->injured_player_id == 0u) {
        return 0;
    }

    uint32_t injured_team_id = 0u;
    uint32_t injured_league_id = 0u;
    uint32_t replacement_team_id = 0u;
    uint32_t replacement_league_id = 0u;
    uint8_t* injured = kbo_find_player_by_id(out->injured_player_id, &injured_team_id, &injured_league_id);
    uint8_t* replacement = kbo_find_player_by_id(out->replacement_player_id, &replacement_team_id, &replacement_league_id);

    if (out->team_id == 0u) {
        out->team_id = injured_team_id != 0u ? injured_team_id : replacement_team_id;
    }
    if (out->league_id == 0u) {
        out->league_id = injured_league_id != 0u ? injured_league_id : replacement_league_id;
    }
    if (out->league_id == 0u && out->team_id != 0u) {
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(out->team_id, 1);
        if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            out->league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        }
    }

    if (out->opened_on_yyyymmdd == 0u) {
        out->opened_on_yyyymmdd = today;
    }
    if ((out->slot_type < KBO_FOREIGN_INJURY_SLOT_REGULAR || out->slot_type > KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA)) {
        if (injured != NULL && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            out->slot_type = kbo_foreign_injury_slot_type_for_player(injured);
        } else if (replacement != NULL && memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
            out->slot_type = kbo_foreign_injury_slot_type_for_player(replacement);
        } else {
            out->slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
        }
    }
    if (out->status < KBO_FOREIGN_INJURY_STATUS_OPEN || out->status > KBO_FOREIGN_INJURY_STATUS_CLOSED) {
        out->status = out->replacement_player_id != 0u ? KBO_FOREIGN_INJURY_STATUS_ACTIVE : KBO_FOREIGN_INJURY_STATUS_OPEN;
    }
    if (out->replacement_player_id == 0u && out->status == KBO_FOREIGN_INJURY_STATUS_ACTIVE) {
        out->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
    }
    if (out->expected_end_yyyymmdd == 0u
            && injured != NULL
            && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
        int16_t days_left = *(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        if (days_left > 0 && today != 0u) {
            out->expected_end_yyyymmdd = kbo_add_days_yyyymmdd(today, (uint32_t)days_left);
        }
    }

    return out->team_id != 0u;
}

static int kbo_import_foreign_injury_replacement_seed_file_locked(
    const char* path,
    uint32_t today,
    const char* source)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 262144u) {
        CloseHandle(file);
        return 0;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int imported = 0;
    int skipped = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[256] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            KboForeignInjuryReplacement rec;
            if (kbo_parse_foreign_injury_replacement_seed_line(line, today, &rec)) {
                if (kbo_find_foreign_injury_replacement_locked(rec.injured_player_id, 1) >= 0) {
                    skipped++;
                } else if (g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
                    g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++] = rec;
                    imported++;
                }
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    if (imported > 0 || skipped > 0) {
        append_logf(
            "foreign injury replacement: seed import source=%s imported=%d skipped=%d path=%s",
            source != NULL ? source : "",
            imported,
            skipped,
            path);
    }
    return imported;
}


/* Foreign injury replacement CSV persistence. Included from native/KBOFix.c. */

static int kbo_persist_foreign_injury_replacements_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign injury replacement: persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }

    DWORD written = 0;
    const char* header = "team_id,league_id,injured_player_id,replacement_player_id,opened_on,expected_end,slot_type,status,converted\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        char line[256] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
            rec->team_id,
            rec->league_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            rec->opened_on_yyyymmdd,
            rec->expected_end_yyyymmdd,
            (uint32_t)rec->slot_type,
            (uint32_t)rec->status,
            (uint32_t)rec->converted);
        if (len > 0 && len < (int)sizeof(line)) {
            written = 0;
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
    }

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        append_logf("foreign injury replacement: atomic commit failed path=%s", path);
        return 0;
    }
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);
    return 1;
}

/* Foreign injury replacement lazy-load orchestration. Included from native/KBOFix.c. */

void kbo_ensure_foreign_injury_replacements_loaded(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return;
    }

    kbo_lock_foreign_injury_replacements();
    if (strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) != 0) {
        kbo_load_foreign_injury_replacements_locked(path);
        uint32_t today = 0u;
        kbo_get_current_yyyymmdd(&today);
        int imported = 0;
        char save_seed_path[MAX_PATH] = {0};
        char global_seed_path[MAX_PATH] = {0};
        if (kbo_get_save_foreign_injury_replacement_seed_path(save_seed_path, sizeof(save_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(save_seed_path, today, "save_seed");
        }
        if (kbo_get_global_foreign_injury_replacement_seed_path(global_seed_path, sizeof(global_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(global_seed_path, today, "global_seed");
        }
        if (imported > 0) {
            kbo_persist_foreign_injury_replacements_locked();
        }
    }
    kbo_unlock_foreign_injury_replacements();
}

/* Foreign injury replacement lookup and counting helpers. Included from native/KBOFix.c. */

static int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed)
{
    if (injured_player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->injured_player_id == injured_player_id
                && (include_closed || rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED)) {
            return i;
        }
    }
    return -1;
}

static int kbo_team_has_foreign_injury_slot_locked(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    if (out_injured_player_id != NULL) {
        *out_injured_player_id = 0u;
    }
    if (team_id == 0u || slot_type == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->team_id == team_id
                && rec->slot_type == slot_type
                && kbo_foreign_injury_status_uses_slot(rec->status)) {
            if (out_injured_player_id != NULL) {
                *out_injured_player_id = rec->injured_player_id;
            }
            return 1;
        }
    }
    return 0;
}

static int kbo_team_has_foreign_injury_slot(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    result = kbo_team_has_foreign_injury_slot_locked(team_id, slot_type, out_injured_player_id);
    kbo_unlock_foreign_injury_replacements();
    return result;
}

int kbo_team_has_foreign_injury_slot_for_candidate_locked(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_replacement_player_id != NULL) { *out_replacement_player_id = 0u; }
    if (team_id == 0u || slot_type == 0u || candidate_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->team_id != team_id
                || rec->slot_type != slot_type
                || !kbo_foreign_injury_status_uses_slot(rec->status)
                || rec->injured_player_id == candidate_player_id) {
            continue;
        }
        if (rec->replacement_player_id != 0u && rec->replacement_player_id != candidate_player_id) {
            continue;
        }
        if (out_injured_player_id != NULL) {
            *out_injured_player_id = rec->injured_player_id;
        }
        if (out_replacement_player_id != NULL) {
            *out_replacement_player_id = rec->replacement_player_id;
        }
        return 1;
    }
    return 0;
}

int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    result = kbo_team_has_foreign_injury_slot_for_candidate_locked(
        team_id,
        slot_type,
        candidate_player_id,
        out_injured_player_id,
        out_replacement_player_id);
    kbo_unlock_foreign_injury_replacements();
    return result;
}

void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed)
{
    if (out_open != NULL) { *out_open = 0; }
    if (out_pending != NULL) { *out_pending = 0; }
    if (out_closed != NULL) { *out_closed = 0; }
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (team_id != 0u && rec->team_id != team_id) {
            continue;
        }
        if (kbo_foreign_injury_status_uses_slot(rec->status)) {
            if (out_open != NULL) { (*out_open)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_PENDING) {
            if (out_pending != NULL) { (*out_pending)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            if (out_closed != NULL) { (*out_closed)++; }
        }
    }
    kbo_unlock_foreign_injury_replacements();
}

/* Foreign injury replacement native news emission. Included from native/KBOFix.c. */

static void kbo_emit_foreign_injury_replacement_news(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase)
{
    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u || rec->league_id == 0u) {
        return;
    }

    uint32_t event_date = 0u;
    if (!kbo_get_current_yyyymmdd(&event_date) || event_date == 0u) {
        event_date = rec->opened_on_yyyymmdd;
    }
    if (event_date == 0u) {
        return;
    }

    char player_name[96] = {0};
    uint8_t* player = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
    if (player != NULL) {
        kbo_copy_player_display_name(player, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", rec->injured_player_id);
    }

    char title[128] = {0};
    char body[1024] = {0};
    if (phase != NULL && strcmp(phase, "pending") == 0) {
        snprintf(title, sizeof(title), "[KBO] Foreign Injury Replacement Decision Required");
        snprintf(
            body,
            sizeof(body),
            "The temporary foreign-player injury replacement window for <Team #%u:team#%u> has moved to a decision stage because <%s:player#%u> is no longer listed as unavailable.\n\nThe club must now close the temporary window or convert the replacement into a regular foreign-player slot under the KBO roster limit.",
            rec->team_id,
            rec->team_id,
            player_name,
            rec->injured_player_id);
    } else {
        snprintf(title, sizeof(title), "[KBO] Foreign Injury Replacement Window Opened");
        snprintf(
            body,
            sizeof(body),
            "The KBO approved a temporary foreign-player injury replacement window for <Team #%u:team#%u> after <%s:player#%u> was diagnosed with an injury expected to keep him out for %d days.\n\nThe club may carry one additional %s foreign player while the injured player remains unavailable.",
            rec->team_id,
            rec->team_id,
            player_name,
            rec->injured_player_id,
            days_left > 0 ? days_left : 0,
            kbo_foreign_injury_slot_label(rec->slot_type));
    }

    int created = create_kbo_native_live_news_with_body(
        event_date / 10000u,
        (event_date / 100u) % 100u,
        event_date % 100u,
        rec->league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "foreign injury replacement: news phase=%s team=%u injured=%u league=%u created=%d",
        phase != NULL ? phase : "open",
        rec->team_id,
        rec->injured_player_id,
        rec->league_id,
        created);
}

/* Foreign injury replacement scanner thread and lifecycle transitions. Included from native/src/foreign_waiver_ai.inc. */

static LONG g_kbo_foreign_injury_replacement_thread_started = 0;

void kbo_foreign_injury_replacement_scan_once(const char* source)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        return;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        return;
    }

    kbo_ensure_foreign_injury_replacements_loaded();

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_resolve_kbo_league_id();
    }

    int scanned = 0;
    int opened = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }
        scanned++;

        uint8_t injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        int16_t days_left = *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        if (!injury_active || days_left < KBO_FOREIGN_INJURY_REPLACEMENT_MIN_DAYS) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (team_id == 0u) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
        if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            if (league_id == 0u) {
                league_id = team_league_id;
            }
        }
        if (configured_league_id != 0u && league_id != 0u && league_id != configured_league_id) {
            continue;
        }

        KboForeignInjuryReplacement created_rec;
        memset(&created_rec, 0, sizeof(created_rec));
        int created = 0;
        kbo_lock_foreign_injury_replacements();
        int existing = kbo_find_foreign_injury_replacement_locked(player_id, 0);
        if (existing < 0 && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++];
            rec->team_id = team_id;
            rec->league_id = league_id != 0u ? league_id : configured_league_id;
            rec->injured_player_id = player_id;
            rec->replacement_player_id = 0u;
            rec->opened_on_yyyymmdd = today;
            rec->expected_end_yyyymmdd = kbo_add_days_yyyymmdd(today, (uint32_t)days_left);
            rec->slot_type = kbo_foreign_injury_slot_type_for_player(player);
            rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
            rec->converted = 0u;
            created_rec = *rec;
            created = kbo_persist_foreign_injury_replacements_locked();
        }
        kbo_unlock_foreign_injury_replacements();

        if (created) {
            opened++;
            kbo_emit_foreign_injury_replacement_news(&created_rec, (int)days_left, "open");
            append_logf(
                "foreign injury replacement: opened source=%s team=%u player=%u league=%u days_left=%d slot=%s",
                source != NULL ? source : "",
                created_rec.team_id,
                created_rec.injured_player_id,
                created_rec.league_id,
                (int)days_left,
                kbo_foreign_injury_slot_label(created_rec.slot_type));
        }
    }

    KboForeignInjuryReplacement pending_news[16];
    int pending_count = 0;
    int changed = 0;
    memset(pending_news, 0, sizeof(pending_news));
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (!kbo_foreign_injury_status_uses_slot(rec->status)) {
            continue;
        }

        uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t injury_active = injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        int16_t days_left = *(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        if (injury_active && days_left > 0) {
            continue;
        }

        rec->status = KBO_FOREIGN_INJURY_STATUS_PENDING;
        if (pending_count < (int)(sizeof(pending_news) / sizeof(pending_news[0]))) {
            pending_news[pending_count++] = *rec;
        }
        changed = 1;
    }
    if (changed) {
        kbo_persist_foreign_injury_replacements_locked();
    }
    kbo_unlock_foreign_injury_replacements();

    for (int i = 0; i < pending_count; i++) {
        kbo_emit_foreign_injury_replacement_news(&pending_news[i], 0, "pending");
        append_logf(
            "foreign injury replacement: pending decision source=%s team=%u player=%u league=%u",
            source != NULL ? source : "",
            pending_news[i].team_id,
            pending_news[i].injured_player_id,
            pending_news[i].league_id);
    }

    if (opened > 0 || pending_count > 0) {
        append_logf(
            "foreign injury replacement: scan source=%s scanned_foreign=%d opened=%d pending=%d",
            source != NULL ? source : "",
            scanned,
            opened,
            pending_count);
    }
}

static DWORD WINAPI kbo_foreign_injury_replacement_thread(LPVOID parameter)
{
    (void)parameter;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(7000)) {
            break;
        }
        kbo_foreign_injury_replacement_scan_once("foreign_injury_replacement_thread");
    }
    InterlockedExchange(&g_kbo_foreign_injury_replacement_thread_started, 0);
    append_log_line("foreign injury replacement thread stopped");
    return 0;
}

void start_kbo_foreign_injury_replacement_thread(void)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        append_log_line("foreign injury replacement: disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_foreign_injury_replacement_thread_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_injury_replacement_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("foreign injury replacement thread started");
    } else {
        InterlockedExchange(&g_kbo_foreign_injury_replacement_thread_started, 0);
        append_log_line("foreign injury replacement thread failed to start");
    }
}


