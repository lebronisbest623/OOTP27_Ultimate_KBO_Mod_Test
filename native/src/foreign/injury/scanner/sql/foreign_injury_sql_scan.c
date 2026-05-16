#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../foreign_injury_scanner_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/league_news/core_sql_league_news.h"
#include "../../../../runtime_memory/runtime_memory.h"

typedef struct KboForeignInjurySqlScan {
    int min_days;
    int found;
    int best_days;
    int rows_seen;
} KboForeignInjurySqlScan;

#define KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_SIZE 512
#define KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_TTL_MS 10000u

typedef struct KboForeignInjurySqlEvidenceCacheEntry {
    uintptr_t database;
    uint32_t player_id;
    int min_days;
    DWORD tick;
    int found;
    int days;
    uint8_t valid;
} KboForeignInjurySqlEvidenceCacheEntry;

static KboForeignInjurySqlEvidenceCacheEntry
    g_kbo_foreign_injury_sql_evidence_cache[KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_SIZE];
static volatile LONG g_kbo_foreign_injury_sql_evidence_cache_lock = 0;

static void kbo_foreign_injury_sql_cache_lock(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_injury_sql_evidence_cache_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_foreign_injury_sql_cache_unlock(void)
{
    InterlockedExchange(&g_kbo_foreign_injury_sql_evidence_cache_lock, 0);
}

static uint32_t kbo_foreign_injury_sql_cache_slot(uint32_t player_id, int min_days)
{
    uint32_t h = player_id * 2654435761u;
    h ^= (uint32_t)min_days * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_SIZE - 1u);
}

static int kbo_foreign_injury_sql_cache_get(
    uintptr_t database,
    uint32_t player_id,
    int min_days,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }

    DWORD now = GetTickCount();
    uint32_t slot = kbo_foreign_injury_sql_cache_slot(player_id, min_days);
    kbo_foreign_injury_sql_cache_lock();
    KboForeignInjurySqlEvidenceCacheEntry cached =
        g_kbo_foreign_injury_sql_evidence_cache[slot];
    kbo_foreign_injury_sql_cache_unlock();

    if (!cached.valid
            || cached.database != database
            || cached.player_id != player_id
            || cached.min_days != min_days
            || now - cached.tick > KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_TTL_MS) {
        return 0;
    }

    if (out_days != NULL) {
        *out_days = cached.days;
    }
    return cached.found ? 1 : -1;
}

static void kbo_foreign_injury_sql_cache_store(
    uintptr_t database,
    uint32_t player_id,
    int min_days,
    int found,
    int days)
{
    uint32_t slot = kbo_foreign_injury_sql_cache_slot(player_id, min_days);
    kbo_foreign_injury_sql_cache_lock();
    KboForeignInjurySqlEvidenceCacheEntry* entry =
        &g_kbo_foreign_injury_sql_evidence_cache[slot];
    entry->valid = 0u;
    entry->database = database;
    entry->player_id = player_id;
    entry->min_days = min_days;
    entry->tick = GetTickCount();
    entry->found = found ? 1 : 0;
    entry->days = days;
    entry->valid = 1u;
    kbo_foreign_injury_sql_cache_unlock();
}

static int __cdecl kbo_foreign_injury_sql_scan_callback(void* arg, int column_count, char** values, char** names)
{
    (void)names;
    KboForeignInjurySqlScan* scan = (KboForeignInjurySqlScan*)arg;
    if (scan == NULL || column_count <= 0 || values == NULL || values[0] == NULL) {
        return 0;
    }

    scan->rows_seen++;
    int evidence_days = 0;
    if (kbo_foreign_injury_duration_text_meets_minimum(values[0], scan->min_days, &evidence_days)) {
        scan->found = 1;
        if (evidence_days > scan->best_days) {
            scan->best_days = evidence_days;
        }
    }
    return 0;
}

int kbo_foreign_injury_recent_sql_has_long_term_injury(
    uint32_t player_id,
    int min_days,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (player_id == 0u || min_days <= 0) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    KboSqlite3ExecFn sqlite_exec = kbo_get_sqlite3_exec_fn();
    if (database == 0 || !memory_range_readable((void*)database, 0x10) || sqlite_exec == NULL) {
        return 0;
    }

    int cached_days = 0;
    int cached = kbo_foreign_injury_sql_cache_get(database, player_id, min_days, &cached_days);
    if (cached != 0) {
        if (out_days != NULL) {
            *out_days = cached_days;
        }
        return cached > 0;
    }

    char player_href[64] = {0};
    snprintf(player_href, sizeof(player_href), "%%/player_%u.html%%", player_id);

    char sql[1600] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT history_text FROM player_history WHERE player_id=%u "
        "UNION ALL SELECT injury_text FROM league_injuries WHERE injury_text LIKE '%s' "
        "UNION ALL SELECT injury_text FROM team_injuries WHERE injury_text LIKE '%s';",
        player_id,
        player_href,
        player_href);

    KboForeignInjurySqlScan scan;
    memset(&scan, 0, sizeof(scan));
    scan.min_days = min_days;
    int result = sqlite_exec(
        (void*)database,
        sql,
        (void*)&kbo_foreign_injury_sql_scan_callback,
        &scan,
        NULL);

    if (scan.found && out_days != NULL) {
        *out_days = scan.best_days;
    }
    if (scan.found) {
        kbo_log_runtimef(
            "foreign injury replacement: sql long-term injury evidence player=%u rows=%d days=%d min_days=%d exec=%d",
            player_id,
            scan.rows_seen,
            scan.best_days,
            min_days,
            result);
    }
    int found = result == 0 && scan.found;
    kbo_foreign_injury_sql_cache_store(database, player_id, min_days, found, scan.best_days);
    return found;
}
