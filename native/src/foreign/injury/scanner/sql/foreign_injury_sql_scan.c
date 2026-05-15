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
    return result == 0 && scan.found;
}
