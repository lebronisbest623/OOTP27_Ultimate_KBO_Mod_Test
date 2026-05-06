/* Core league news SQL persistence. */

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

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_logf("league news sql skipped source=%s title=%s reason=no_exe_module", source != NULL ? source : "", title);
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)global, OOTP27_GLOBAL_SQL_ENABLED_OFFSET + sizeof(uint8_t))) {
        append_logf("league news sql skipped source=%s title=%s reason=no_global_database", source != NULL ? source : "", title);
        return 0;
    }

    uint8_t sql_enabled = *(uint8_t*)(global + OOTP27_GLOBAL_SQL_ENABLED_OFFSET);
    if (sql_enabled == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        append_logf(
            "league news sql skipped source=%s title=%s reason=sql_disabled_or_unreadable enabled=%u",
            source != NULL ? source : "",
            title,
            (uint32_t)sql_enabled);
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    if (database == 0 || !memory_range_readable((void*)database, 8)) {
        append_logf("league news sql skipped source=%s title=%s reason=no_sql_database db=%p", source != NULL ? source : "", title, (void*)database);
        return 0;
    }

    OotpSqlExecFn sql_exec = (OotpSqlExecFn)((uintptr_t)exe + OOTP27_SQL_EXEC_DIRECT_RVA);
    if (!memory_range_readable((void*)sql_exec, 16)) {
        append_logf("league news sql skipped source=%s title=%s reason=sql_exec_unreadable fn=%p", source != NULL ? source : "", title, (void*)sql_exec);
        return 0;
    }

    char escaped_title[512] = {0};
    char escaped_body[8192] = {0};
    char news_date[32] = {0};
    if (!kbo_sql_escape_literal(escaped_title, sizeof(escaped_title), title)
            || !kbo_sql_escape_literal(escaped_body, sizeof(escaped_body), body != NULL ? body : "")
            || !kbo_format_history_date(news_date, sizeof(news_date), year, month, day)) {
        append_logf("league news sql skipped source=%s title=%s reason=escape_or_date_failed", source != NULL ? source : "", title);
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

    int delete_result = sql_exec((void*)database, delete_sql);
    int insert_result = sql_exec((void*)database, insert_sql);
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

    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t global = get_ootp_global_database();
    if (exe == NULL || global == 0
            || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        append_logf("league_news table sql skipped source=%s title=%s reason=no_exe_or_global", source != NULL ? source : "", title);
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    if (database == 0 || !memory_range_readable((void*)database, 0x10)) {
        append_logf("league_news table sql skipped source=%s title=%s reason=no_database db=%p", source != NULL ? source : "", title, (void*)database);
        return 0;
    }

    OotpSqlExecFn sql_exec = (OotpSqlExecFn)((uintptr_t)exe + OOTP27_SQL_EXEC_DIRECT_RVA);
    if (!memory_range_readable((void*)sql_exec, 16)) {
        append_logf("league_news table sql skipped source=%s title=%s reason=exec_unreadable fn=%p", source != NULL ? source : "", title, (void*)sql_exec);
        return 0;
    }

    char escaped_text[8192] = {0};
    char escaped_title[512] = {0};
    char escaped_body[7600] = {0};
    char news_date[16] = {0};
    if (!kbo_sql_escape_literal(escaped_title, sizeof(escaped_title), title)
            || !kbo_sql_escape_literal(escaped_body, sizeof(escaped_body), body != NULL ? body : "")
            || !kbo_format_history_date(news_date, sizeof(news_date), year, month, day)) {
        append_logf("league_news table sql skipped source=%s title=%s reason=escape_or_date_failed", source != NULL ? source : "", title);
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

    int create_result = sql_exec((void*)database, create_sql);
    int delete_result = sql_exec((void*)database, delete_sql);
    int insert_result = sql_exec((void*)database, insert_sql);
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
        (void*)sql_exec);
    return insert_result != 0;
}
