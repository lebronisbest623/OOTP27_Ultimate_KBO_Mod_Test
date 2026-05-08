#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_atomic_file.h"
#include "../../core/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/team_lookup.h"
#include "../../team/team_roster_arrays.h"
#include "../foreign_csv_parse.h"
#include "../foreign_waiver_paths.h"
#include "../foreign_waiver_player_eval.h"
#include "foreign_waiver_rights_query.h"

LONG g_kbo_foreign_waiver_rights_lock = 0;
KboForeignWaiverRetention g_kbo_foreign_waiver_rights[KBO_FOREIGN_WAIVER_RIGHTS_MAX] = {{0}};
int g_kbo_foreign_waiver_rights_count = 0;
/* ---- native/src/foreign/rights/foreign_waiver_rights_active.inc ---- */
/* Foreign reserve-right active-state helpers. */

int kbo_is_foreign_waiver_right_active(const KboForeignWaiverRetention* rec, uint32_t today)
{
    if (rec == NULL
            || rec->player_id == 0u
            || rec->team_id == 0u
            || rec->retained_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd < rec->retained_on_yyyymmdd
            || today == 0u) {
        return 0;
    }
    return rec->retained_on_yyyymmdd <= today && today <= rec->expires_on_yyyymmdd;
}

static int kbo_is_foreign_waiver_right_expired(const KboForeignWaiverRetention* rec, uint32_t today)
{
    if (rec == NULL
            || rec->player_id == 0u
            || rec->team_id == 0u
            || rec->retained_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd < rec->retained_on_yyyymmdd
            || today == 0u) {
        return 1;
    }
    return today > rec->expires_on_yyyymmdd;
}

/* ---- native/src/foreign/rights/foreign_waiver_rights_persist.inc ---- */
/* Foreign reserve-right CSV persistence. */

int kbo_persist_foreign_waiver_rights(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        append_log_line("foreign reserve rights: persist skipped reason=path_unavailable");
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
        append_logf("foreign reserve rights: persist failed reason=create_tmp gle=%lu path=%s", GetLastError(), path);
        return 0;
    }
    char header[64] = "player_id,team_id,league_id,retained_on,expires_on\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == 0u || rec->team_id == 0u) {
            continue;
        }
        char line[128] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u\r\n",
            rec->player_id,
            rec->team_id,
            rec->league_id,
            rec->retained_on_yyyymmdd,
            rec->expires_on_yyyymmdd);
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    int ok = kbo_atomic_commit(file, tmp_path, path);
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    if (!ok) {
        append_logf("foreign reserve rights: atomic commit failed path=%s", path);
        return 0;
    }
    append_logf("foreign reserve rights: persisted=%d path=%s", g_kbo_foreign_waiver_rights_count, path);
    return 1;
}

/* ---- native/src/foreign/rights/foreign_waiver_rights_load.inc ---- */
/* Foreign reserve-right CSV loading. */

int kbo_load_foreign_waiver_rights(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD file_size = GetFileSize(file, NULL);
    if (file_size == INVALID_FILE_SIZE || file_size == 0) {
        CloseHandle(file);
        return 0;
    }
    char raw[65536] = {0};
    DWORD read = 0;
    if (file_size >= sizeof(raw)) {
        file_size = sizeof(raw) - 1u;
    }
    ReadFile(file, raw, file_size, &read, NULL);
    CloseHandle(file);
    if (read == 0u) {
        return 0;
    }
    raw[read] = '\0';

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    g_kbo_foreign_waiver_rights_count = 0;
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    char* cursor = raw;
    int deduped = 0;
    while (*cursor != '\0') {
        char* next = strchr(cursor, '\n');
        if (next == NULL) {
            next = cursor + strlen(cursor);
        }
        size_t len = (size_t)(next - cursor);
        while (len > 0 && (cursor[len - 1] == '\r' || cursor[len - 1] == '\n')) {
            len--;
        }
        if (len > 8) {
            char line[128] = {0};
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);
            const char* p = line;
            if (line[0] != '#' && line[0] != ';') {
                uint32_t player_id = 0;
                uint32_t team_id = 0;
                uint32_t league_id = 0;
                uint32_t retained_on = 0;
                uint32_t expires_on = 0;
                if (parse_u32_from_csv_field((const char**)&p, &player_id)
                    && parse_u32_from_csv_field((const char**)&p, &team_id)
                    && parse_u32_from_csv_field((const char**)&p, &league_id)
                    && parse_u32_from_csv_field((const char**)&p, &retained_on)
                    && parse_u32_from_csv_field((const char**)&p, &expires_on)) {
                    if (player_id != 0u && team_id != 0u && league_id != 0u) {
                        while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
                            Sleep(0);
                        }
                        int existing_index = -1;
                        for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
                            if (g_kbo_foreign_waiver_rights[i].player_id == player_id) {
                                existing_index = i;
                                break;
                            }
                        }
                        if (existing_index >= 0) {
                            KboForeignWaiverRetention* existing = &g_kbo_foreign_waiver_rights[existing_index];
                            if (retained_on >= existing->retained_on_yyyymmdd) {
                                *existing = (KboForeignWaiverRetention){
                                    player_id,
                                    team_id,
                                    league_id,
                                    retained_on,
                                    expires_on
                                };
                            }
                            deduped++;
                        } else if (g_kbo_foreign_waiver_rights_count < KBO_FOREIGN_WAIVER_RIGHTS_MAX) {
                            g_kbo_foreign_waiver_rights[g_kbo_foreign_waiver_rights_count++] = (KboForeignWaiverRetention){
                                player_id,
                                team_id,
                                league_id,
                                retained_on,
                                expires_on
                            };
                        }
                        InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
                    }
                }
            }
        }
        if (*next == '\0') {
            break;
        }
        cursor = next + 1;
    }
    append_logf("foreign reserve rights: loaded=%d deduped=%d path=%s", g_kbo_foreign_waiver_rights_count, deduped, path);
    if (deduped > 0) {
        kbo_persist_foreign_waiver_rights();
    }
    return 1;
}

/* ---- native/src/foreign/rights/foreign_waiver_rights_mutation.inc ---- */
/* Foreign reserve-right table pruning and upserts. */

void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd)
{
    if (today_yyyymmdd == 0u) {
        return;
    }
    static volatile LONG last_pruned_today = 0;
    LONG today_long = (LONG)today_yyyymmdd;
    if (InterlockedCompareExchange(&last_pruned_today, today_long, today_long) == today_long) {
        return;
    }
    LONG previous = InterlockedExchange(&last_pruned_today, today_long);
    if (previous == today_long) {
        return;
    }
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    int w = 0;
    int removed = 0;
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (kbo_is_foreign_waiver_right_expired(&g_kbo_foreign_waiver_rights[i], today_yyyymmdd)) {
            removed++;
            continue;
        }
        if (w != i) {
            g_kbo_foreign_waiver_rights[w] = g_kbo_foreign_waiver_rights[i];
        }
        w++;
    }
    for (int i = w; i < g_kbo_foreign_waiver_rights_count; i++) {
        memset(&g_kbo_foreign_waiver_rights[i], 0, sizeof(KboForeignWaiverRetention));
    }
    g_kbo_foreign_waiver_rights_count = w;
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    if (removed > 0) {
        append_logf("foreign reserve rights: expired=%d today=%u", removed, today_yyyymmdd);
        kbo_persist_foreign_waiver_rights();
    }
}

int kbo_set_foreign_waiver_right(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t league_id,
    uint32_t retained_on,
    uint32_t expires_on)
{
    if (team_id == 0u || player_id == 0u || league_id == 0u || retained_on == 0u || expires_on == 0u) {
        return 0;
    }

    int upserted = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (g_kbo_foreign_waiver_rights[i].player_id == player_id) {
            if (g_kbo_foreign_waiver_rights[i].team_id != team_id) {
                append_logf(
                    "foreign reserve rights: reassigned exclusive holder player=%u old_team=%u new_team=%u",
                    player_id,
                    g_kbo_foreign_waiver_rights[i].team_id,
                    team_id);
            }
            g_kbo_foreign_waiver_rights[i].team_id = team_id;
            g_kbo_foreign_waiver_rights[i].league_id = league_id;
            g_kbo_foreign_waiver_rights[i].retained_on_yyyymmdd = retained_on;
            g_kbo_foreign_waiver_rights[i].expires_on_yyyymmdd = expires_on;
            upserted = 1;
            break;
        }
    }
    if (!upserted && g_kbo_foreign_waiver_rights_count < KBO_FOREIGN_WAIVER_RIGHTS_MAX) {
        g_kbo_foreign_waiver_rights[g_kbo_foreign_waiver_rights_count++] = (KboForeignWaiverRetention){
            player_id,
            team_id,
            league_id,
            retained_on,
            expires_on
        };
        upserted = 1;
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (upserted) {
        kbo_persist_foreign_waiver_rights();
    }
    return upserted;
}

int kbo_clear_foreign_waiver_right(uint32_t team_id, uint32_t player_id)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }

    int removed = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    int w = 0;
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id && rec->player_id == player_id) {
            removed++;
            continue;
        }
        if (w != i) {
            g_kbo_foreign_waiver_rights[w] = g_kbo_foreign_waiver_rights[i];
        }
        w++;
    }
    for (int i = w; i < g_kbo_foreign_waiver_rights_count; i++) {
        memset(&g_kbo_foreign_waiver_rights[i], 0, sizeof(KboForeignWaiverRetention));
    }
    g_kbo_foreign_waiver_rights_count = w;
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (removed > 0) {
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
        if (team != NULL) {
            kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id);
        }

        uint32_t current_team_id = 0u;
        uint32_t current_league_id = 0u;
        uint8_t* player = kbo_find_player_by_id(player_id, &current_team_id, &current_league_id);
        if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
            player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;
            if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == team_id) {
                *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
            }
        }

        append_logf("foreign reserve rights: released team=%u player=%u removed=%d", team_id, player_id, removed);
        kbo_persist_foreign_waiver_rights();
    }
    return removed;
}

/* ---- native/src/foreign/rights/foreign_waiver_rights_query.inc ---- */
/* Foreign reserve-right lookup helpers. */

static void kbo_ensure_foreign_waiver_rights_loaded_for_lookup(void)
{
    static volatile LONG rights_loaded = 0;
    static volatile LONG load_in_progress = 0;
    static volatile LONG last_attempt_tick = 0;

    if (InterlockedCompareExchange(&rights_loaded, 0, 0) == 1) {
        return;
    }

    DWORD now = GetTickCount();
    LONG last = InterlockedCompareExchange(&last_attempt_tick, 0, 0);
    if (last != 0 && now - (DWORD)last < 1000u) {
        return;
    }

    if (InterlockedCompareExchange(&load_in_progress, 1, 0) != 0) {
        return;
    }

    InterlockedExchange(&last_attempt_tick, (LONG)now);
    if (kbo_load_foreign_waiver_rights()) {
        InterlockedExchange(&rights_loaded, 1);
    }
    InterlockedExchange(&load_in_progress, 0);
}

int kbo_has_active_foreign_waiver_right(uint32_t team_id, uint32_t player_id, uint32_t today_yyyymmdd)
{
    if (team_id == 0u || player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    int result = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id && rec->player_id == player_id && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            result = 1;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    return result;
}

int kbo_get_active_foreign_waiver_right_dates(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t today_yyyymmdd,
    uint32_t* out_retained_on,
    uint32_t* out_expires_on)
{
    if (out_retained_on != NULL) {
        *out_retained_on = 0u;
    }
    if (out_expires_on != NULL) {
        *out_expires_on = 0u;
    }
    if (team_id == 0u || player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }

    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();

    int result = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id
                && rec->player_id == player_id
                && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            if (out_retained_on != NULL) {
                *out_retained_on = rec->retained_on_yyyymmdd;
            }
            if (out_expires_on != NULL) {
                *out_expires_on = rec->expires_on_yyyymmdd;
            }
            result = 1;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
    return result;
}

int kbo_find_active_foreign_waiver_holder(uint32_t player_id, uint32_t today_yyyymmdd, uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0;
    }
    if (player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }

    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();

    enum { KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE = 256 };
    typedef struct KboForeignWaiverHolderCacheEntry {
        uint32_t player_id;
        uint32_t today_yyyymmdd;
        uint32_t holder_team_id;
        uint8_t found;
        DWORD tick;
    } KboForeignWaiverHolderCacheEntry;
    static KboForeignWaiverHolderCacheEntry holder_cache[KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE] = {{0}};

    DWORD now = GetTickCount();
    uint32_t slot_index = (player_id ^ (player_id >> 8) ^ today_yyyymmdd) % KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE;
    KboForeignWaiverHolderCacheEntry* cached = &holder_cache[slot_index];
    if (cached->player_id == player_id
            && cached->today_yyyymmdd == today_yyyymmdd
            && cached->tick != 0u
            && now - cached->tick <= 500u) {
        if (cached->found && out_team_id != NULL) {
            *out_team_id = cached->holder_team_id;
        }
        return cached->found ? 1 : 0;
    }

    int found = 0;
    uint32_t holder_team_id = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == player_id && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            holder_team_id = rec->team_id;
            found = holder_team_id != 0u;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (found && out_team_id != NULL) {
        *out_team_id = holder_team_id;
    }
    cached->player_id = player_id;
    cached->today_yyyymmdd = today_yyyymmdd;
    cached->holder_team_id = holder_team_id;
    cached->found = found ? 1u : 0u;
    cached->tick = now;
    return found;
}

/* ---- native/src/foreign/rights/foreign_waiver_rights_memory_sync.inc ---- */
/* Foreign reserve-right memory synchronization. */

int kbo_sync_active_foreign_waiver_right_to_memory(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    uint32_t today_yyyymmdd)
{
    if (player == NULL || player_id == 0u || holder_team_id == 0u || today_yyyymmdd == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t league_id = 0u;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == player_id
                && rec->team_id == holder_team_id
                && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            league_id = rec->league_id;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    uint8_t* holder_team = find_kbo_team_by_numeric_id_any_league(holder_team_id, 1);
    if (holder_team == NULL || !memory_range_readable(holder_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    if (league_id == 0u) {
        league_id = *(uint32_t*)(holder_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    if (league_id == 0u) {
        return 0;
    }

    int changed = 0;
    if (player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != holder_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = holder_team_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (kbo_add_player_id_to_team_fixed_array(holder_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)) {
        changed = 1;
    }

    if (changed) {
        static LONG sync_log_count = 0;
        LONG slot = InterlockedIncrement(&sync_log_count);
        if (slot <= 200) {
            append_logf(
                "foreign reserve rights: synced active right to memory player=%u holder_team=%u league=%u today=%u",
                player_id,
                holder_team_id,
                league_id,
                today_yyyymmdd);
        }
    }
    return 1;
}

