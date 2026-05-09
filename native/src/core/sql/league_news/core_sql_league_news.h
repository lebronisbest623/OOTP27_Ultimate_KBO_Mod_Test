#ifndef KBO_CORE_SQL_LEAGUE_NEWS_H
#define KBO_CORE_SQL_LEAGUE_NEWS_H

#include <stdint.h>

typedef int (__cdecl *KboSqlite3ExecFn)(void* database, const char* sql, void* callback, void* callback_arg, char** errmsg);

KboSqlite3ExecFn kbo_get_sqlite3_exec_fn(void);
int kbo_sqlite_exec_direct(void* database, const char* sql);

int insert_kbo_league_news_sql(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body,
    const char* source);

int insert_kbo_league_news_table_sql(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    const char* title,
    const char* body,
    const char* source);

#endif
