#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cbt_draft_probe.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sql/league_news/core_sql_league_news.h"

typedef int (__cdecl *KboSqlite3ExecCbFn)(void*, int, char**, char**);

static int kbo_cbt_schema_row_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)user_data;
    (void)names;
    char line[1024] = {0};
    size_t pos = 0;
    for (int i = 0; i < ncols && pos < sizeof(line) - 4; i++) {
        const char* v = vals[i] != NULL ? vals[i] : "NULL";
        size_t vlen = strlen(v);
        if (pos + vlen + 3 >= sizeof(line)) {
            break;
        }
        if (i > 0) {
            line[pos++] = '|';
        }
        memcpy(line + pos, v, vlen);
        pos += vlen;
    }
    line[pos] = '\0';
    kbo_log_runtimef("CBT_SCHEMA: %s", line);
    return 0;
}

void kbo_cbt_draft_probe_schema(void* database)
{
    if (database == NULL) {
        return;
    }

    KboSqlite3ExecFn exec = kbo_get_sqlite3_exec_fn();
    if (exec == NULL) {
        return;
    }

    kbo_log_runtimef("CBT_SCHEMA_PROBE: dumping draft/pick tables from sqlite_master");

    /* All table names */
    exec(database,
        "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;",
        (void*)(KboSqlite3ExecCbFn)kbo_cbt_schema_row_cb, NULL, NULL);

    /* Full CREATE TABLE SQL for draft/pick tables */
    exec(database,
        "SELECT name, sql FROM sqlite_master WHERE type='table'"
        " AND (name LIKE '%draft%' OR name LIKE '%pick%' OR name LIKE '%amateur%') ORDER BY name;",
        (void*)(KboSqlite3ExecCbFn)kbo_cbt_schema_row_cb, NULL, NULL);

    kbo_log_runtimef("CBT_SCHEMA_PROBE: done");
}
