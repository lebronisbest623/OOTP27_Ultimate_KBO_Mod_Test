/* Core league news SQL persistence. */

typedef int (__cdecl *KboSqlite3ExecFn)(void* database, const char* sql, void* callback, void* callback_arg, char** errmsg);

static KboSqlite3ExecFn kbo_get_sqlite3_exec_fn(void)
{
    static KboSqlite3ExecFn cached_exec = NULL;
    static LONG attempted = 0;
    if (InterlockedCompareExchange(&attempted, 1, 1) != 0) {
        return cached_exec;
    }

    HMODULE module = GetModuleHandleA("sqlite3.dll");
    if (module == NULL) {
        module = GetModuleHandleA("SQLite3.dll");
    }
    if (module == NULL) {
        module = GetModuleHandleA(NULL);
    }

    if (module != NULL) {
        FARPROC proc = GetProcAddress(module, "sqlite3_exec");
        if (proc != NULL) {
            cached_exec = (KboSqlite3ExecFn)proc;
        }
    }
    InterlockedExchange(&attempted, 1);
    return cached_exec;
}

static int kbo_sqlite_exec_direct(void* database, const char* sql)
{
    KboSqlite3ExecFn sqlite_exec = kbo_get_sqlite3_exec_fn();
    if (sqlite_exec == NULL || database == NULL || sql == NULL) {
        return 0;
    }
    return sqlite_exec(database, sql, NULL, NULL, NULL) == 0 ? 1 : 0;
}

static int kbo_copy_log_string_safe(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (source == NULL || !memory_range_readable(source, 1)) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < 256u; len++) {
        if (!memory_range_readable(source + len, 1)) {
            break;
        }
        char c = source[len];
        if (c == '\0') {
            break;
        }
        if ((unsigned char)c < 0x20u) {
            c = ' ';
        }
        out[len] = c;
    }
    out[len] = '\0';
    return len > 0;
}

static int insert_kbo_league_news_sql(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body,
    const char* source)
{
    if (title == NULL || title[0] == '\0' || league_id == 0
            || year < 1800 || year > 2300 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    char log_title[192] = {0};
    char log_source[96] = {0};
    kbo_copy_log_string_safe(title, log_title, sizeof(log_title));
    kbo_copy_log_string_safe(source != NULL ? source : "", log_source, sizeof(log_source));

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)global, OOTP27_GLOBAL_SQL_ENABLED_OFFSET + sizeof(uint8_t))) {
        append_logf("league news sql skipped source=%s title=%s reason=no_global_database", log_source, log_title);
        return 0;
    }

    uint8_t sql_enabled = *(uint8_t*)(global + OOTP27_GLOBAL_SQL_ENABLED_OFFSET);
    if (sql_enabled == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        append_logf(
            "league news sql skipped source=%s title=%s reason=sql_disabled_or_unreadable enabled=%u",
            log_source,
            log_title,
            (uint32_t)sql_enabled);
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    if (database == 0 || !memory_range_readable((void*)database, 8)) {
        append_logf("league news sql skipped source=%s title=%s reason=no_sql_database db=%p", log_source, log_title, (void*)database);
        return 0;
    }

    KboSqlite3ExecFn sqlite_exec = kbo_get_sqlite3_exec_fn();
    if (sqlite_exec == NULL) {
        append_logf("league news sql skipped source=%s title=%s reason=sqlite3_exec_unavailable", log_source, log_title);
        return 0;
    }

    char escaped_title[512] = {0};
    char escaped_body[8192] = {0};
    char news_date[32] = {0};
    if (!kbo_sql_escape_literal(escaped_title, sizeof(escaped_title), title)
            || !kbo_sql_escape_literal(escaped_body, sizeof(escaped_body), body != NULL ? body : "")
            || !kbo_format_history_date(news_date, sizeof(news_date), year, month, day)) {
        append_logf("league news sql skipped source=%s title=%s reason=escape_or_date_failed", log_source, log_title);
        return 0;
    }

    uint32_t message_id = 8000000u
        + ((year % 100u) * 10000u)
        + (month * 100u)
        + day;
    if (title != NULL && strstr(title, "Depart") != NULL) {
        message_id += 100000u;
    } else if (title != NULL && (strstr(title, "Return") != NULL || strstr(title, "Gold") != NULL || strstr(title, "Win") != NULL)) {
        message_id += 200000u;
    }

    char delete_sql[1400] = {0};
    snprintf(
        delete_sql,
        sizeof(delete_sql),
        "DELETE FROM messages WHERE message_id=%u OR (subject='%s' AND date='%s' AND league_id_0=%u AND sender_type=9001);",
        message_id,
        escaped_title,
        news_date,
        league_id);

    char insert_sql[12000] = {0};
    snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO messages(message_id, subject, player_id_0, player_id_1, player_id_2, player_id_3, player_id_4, team_id_0, team_id_1, team_id_2, team_id_3, team_id_4, league_id_0, league_id_1, importance, message_type, hype, sender_type, sender_id, recipient_id, trade_id, date, deleted, notify, ongoing_story_id, text_is_modified, body) "
        "VALUES(%u, '%s', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, %u, 0, 3, %u, 0, 9001, %u, 0, 0, '%s', 0, 1, 0, 1, '%s');",
        message_id,
        escaped_title,
        league_id,
        message_type,
        league_id,
        news_date,
        escaped_body);

    int delete_result = sqlite_exec((void*)database, delete_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int insert_result = sqlite_exec((void*)database, insert_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int body_file = 0;
    if (insert_result != 0) {
        body_file = write_kbo_message_body_file(message_id, title, body, source);
    }
    append_logf(
        "league news sql insert source=%s title=%s date=%s league_id=%u type=%u message_id=%u delete_result=%d insert_result=%d body_file=%d db=%p",
        source != NULL ? source : "",
        title,
        news_date,
        league_id,
        message_type,
        message_id,
        delete_result,
        insert_result,
        body_file,
        (void*)database);

    return insert_result != 0;
}

static int insert_kbo_league_news_table_sql(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    const char* title,
    const char* body,
    const char* source)
{
    if (title == NULL || title[0] == '\0' || league_id == 0
            || year < 1800 || year > 2300 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    char log_title[192] = {0};
    char log_source[96] = {0};
    kbo_copy_log_string_safe(title, log_title, sizeof(log_title));
    kbo_copy_log_string_safe(source != NULL ? source : "", log_source, sizeof(log_source));

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        append_logf("league_news table sql skipped source=%s title=%s reason=no_global", log_source, log_title);
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    if (database == 0 || !memory_range_readable((void*)database, 0x10)) {
        append_logf("league_news table sql skipped source=%s title=%s reason=no_database db=%p", log_source, log_title, (void*)database);
        return 0;
    }

    KboSqlite3ExecFn sqlite_exec = kbo_get_sqlite3_exec_fn();
    if (sqlite_exec == NULL) {
        append_logf("league_news table sql skipped source=%s title=%s reason=sqlite3_exec_unavailable", log_source, log_title);
        return 0;
    }

    char escaped_text[8192] = {0};
    char escaped_title[512] = {0};
    char escaped_body[7600] = {0};
    char news_date[16] = {0};
    if (!kbo_sql_escape_literal(escaped_title, sizeof(escaped_title), title)
            || !kbo_sql_escape_literal(escaped_body, sizeof(escaped_body), body != NULL ? body : "")
            || !kbo_format_history_date(news_date, sizeof(news_date), year, month, day)) {
        append_logf("league_news table sql skipped source=%s title=%s reason=escape_or_date_failed", log_source, log_title);
        return 0;
    }

    snprintf(
        escaped_text,
        sizeof(escaped_text),
        "%s%s%s",
        escaped_title,
        escaped_body[0] != '\0' ? "\n\n" : "",
        escaped_body);

    const char* create_sql =
        "CREATE TABLE IF NOT EXISTS league_news (news_id INTEGER PRIMARY KEY AUTOINCREMENT, league_id INTEGER, news_date VARCHAR(8), news_text TEXT, season INTEGER);"
        "CREATE INDEX IF NOT EXISTS league_news_league_id ON league_news(league_id);"
        "CREATE INDEX IF NOT EXISTS league_news_date ON league_news(news_date);";

    char delete_sql[9200] = {0};
    snprintf(
        delete_sql,
        sizeof(delete_sql),
        "DELETE FROM league_news WHERE league_id=%u AND news_date='%s' AND news_text='%s';",
        league_id,
        news_date,
        escaped_text);

    char insert_sql[10000] = {0};
    snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO league_news(league_id, news_date, news_text, season) VALUES(%u, '%s', '%s', %u);",
        league_id,
        news_date,
        escaped_text,
        year);

    int create_result = sqlite_exec((void*)database, create_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int delete_result = sqlite_exec((void*)database, delete_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int insert_result = sqlite_exec((void*)database, insert_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    append_logf(
        "league_news table sql insert source=%s title=%s date=%s league_id=%u create=%d delete=%d insert=%d db=%p exec=%p",
        source != NULL ? source : "",
        title,
        news_date,
        league_id,
        create_result,
        delete_result,
        insert_result,
        (void*)database,
        (void*)sqlite_exec);
    return insert_result != 0;
}
