static void append_log_line(const char* message)
{
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s\\OOTP-KBO", local_app_data);
    CreateDirectoryA(dir, NULL);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\kbofix.log", dir);

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);

    char line[4096] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu %s\r\n",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds,
        GetCurrentProcessId(),
        message);

    if (len > 0) {
        DWORD written = 0;
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }

    CloseHandle(file);
}

static void append_logf(const char* format, ...)
{
    char message[3072] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    append_log_line(message);
}

static int ascii_equals_ignore_case(const char* a, const char* b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static int kbo_is_leap_year(uint32_t year)
{
    return (year % 4u == 0u && year % 100u != 0u) || (year % 400u == 0u);
}

static int kbo_format_history_date(char* out, size_t out_size, uint32_t year, uint32_t month, uint32_t day)
{
    static const uint8_t month_days_common[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (out == NULL || out_size < 9 || year < 1800 || year > 2200
            || month < 1 || month > 12 || day < 1) {
        return 0;
    }

    uint32_t max_day = month_days_common[month - 1u];
    if (month == 2 && kbo_is_leap_year(year)) {
        max_day = 29;
    }
    if (day > max_day) {
        return 0;
    }

    snprintf(out, out_size, "%04u%02u%02u", year, month, day);
    return 1;
}

static int kbo_sql_escape_literal(char* out, size_t out_size, const char* in)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }

    size_t write = 0;
    if (in != NULL) {
        for (const char* p = in; *p != '\0'; p++) {
            if ((unsigned char)*p < 0x20 && *p != '\t') {
                continue;
            }

            size_t needed = (*p == '\'') ? 2u : 1u;
            if (write + needed >= out_size) {
                out[0] = '\0';
                return 0;
            }

            if (*p == '\'') {
                out[write++] = '\'';
                out[write++] = '\'';
            } else {
                out[write++] = *p;
            }
        }
    }

    out[write] = '\0';
    return 1;
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

    OotpSqlExecFn sql_exec = (OotpSqlExecFn)((uintptr_t)exe + OOTP27_SQL_EXEC_RVA);
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

    char delete_sql[1400] = {0};
    snprintf(
        delete_sql,
        sizeof(delete_sql),
        "DELETE FROM messages WHERE subject='%s' AND date='%s' AND league_id_0=%u AND sender_type=9001;",
        escaped_title,
        news_date,
        league_id);

    char insert_sql[12000] = {0};
    snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO messages(message_id, subject, player_id_0, player_id_1, player_id_2, player_id_3, player_id_4, team_id_0, team_id_1, team_id_2, team_id_3, team_id_4, league_id_0, league_id_1, importance, message_type, hype, sender_type, sender_id, recipient_id, trade_id, date, deleted, notify, ongoing_story_id, text_is_modified, body) "
        "SELECT COALESCE(MAX(message_id),0)+1, '%s', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, %u, 0, 3, %u, 0, 9001, %u, 0, 0, '%s', 0, 1, 0, 1, '%s' FROM messages;",
        escaped_title,
        league_id,
        message_type,
        league_id,
        news_date,
        escaped_body);

    int delete_result = sql_exec((void*)database, delete_sql);
    int insert_result = sql_exec((void*)database, insert_sql);
    append_logf(
        "league news sql insert source=%s title=%s date=%s league_id=%u type=%u delete_result=%d insert_result=%d db=%p",
        source != NULL ? source : "",
        title,
        news_date,
        league_id,
        message_type,
        delete_result,
        insert_result,
        (void*)database);

    return insert_result != 0;
}

static int kbo_path_looks_like_absolute_save_path(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
            && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        return 1;
    }
    return path[0] == '\\' && path[1] == '\\';
}

static int find_recent_ootp27_save_with_messages(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char user_profile[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("USERPROFILE", user_profile, (DWORD)sizeof(user_profile));
    if (got == 0 || got >= sizeof(user_profile)) {
        return 0;
    }

    char pattern[MAX_PATH] = {0};
    snprintf(
        pattern,
        sizeof(pattern),
        "%s\\Documents\\Out of the Park Developments\\OOTP Baseball 27\\saved_games\\*.lg",
        user_profile);

    WIN32_FIND_DATAA data;
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    ULONGLONG best_time = 0;
    char best_path[MAX_PATH] = {0};
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                || strcmp(data.cFileName, ".") == 0
                || strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        char candidate[MAX_PATH] = {0};
        snprintf(
            candidate,
            sizeof(candidate),
            "%s\\Documents\\Out of the Park Developments\\OOTP Baseball 27\\saved_games\\%s",
            user_profile,
            data.cFileName);

        char message_dir[MAX_PATH] = {0};
        snprintf(message_dir, sizeof(message_dir), "%s\\messages", candidate);
        DWORD attrs = GetFileAttributesA(message_dir);
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }

        ULONGLONG write_time = ((ULONGLONG)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
        if (write_time > best_time) {
            best_time = write_time;
            snprintf(best_path, sizeof(best_path), "%s", candidate);
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);

    if (best_path[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s", best_path);
    return 1;
}

static int write_kbo_message_body_file(uint32_t message_id, const char* title, const char* body, const char* source)
{
    if (message_id == 0 || title == NULL || title[0] == '\0') {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    char save_path[MAX_PATH] = {0};
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_MESSAGE_SAVE_PATH_OFFSET), 0x20)) {
        copy_ootp_string_object_text((uint8_t*)global, OOTP27_MESSAGE_SAVE_PATH_OFFSET, save_path, sizeof(save_path));
    }

    if (!kbo_path_looks_like_absolute_save_path(save_path)) {
        char fallback_save_path[MAX_PATH] = {0};
        if (find_recent_ootp27_save_with_messages(fallback_save_path, sizeof(fallback_save_path))) {
            append_logf(
                "league news body file save path fallback source=%s title=%s old=%s new=%s",
                source != NULL ? source : "",
                title,
                save_path,
                fallback_save_path);
            snprintf(save_path, sizeof(save_path), "%s", fallback_save_path);
        } else {
            save_path[0] = '\0';
        }
    }

    if (save_path[0] == '\0') {
        append_logf("league news body file skipped source=%s title=%s reason=no_save_path", source != NULL ? source : "", title);
        return 0;
    }

    char message_dir[MAX_PATH] = {0};
    snprintf(message_dir, sizeof(message_dir), "%s\\messages", save_path);
    CreateDirectoryA(message_dir, NULL);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\message%u.txt", message_dir, message_id);

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf(
            "league news body file skipped source=%s title=%s id=%u reason=create_failed gle=%lu path=%s",
            source != NULL ? source : "",
            title,
            message_id,
            GetLastError(),
            path);
        return 0;
    }

    char text[4096] = {0};
    int len = snprintf(text, sizeof(text), "%s\r\n%s\r\n", title, body != NULL ? body : "");
    DWORD written = 0;
    BOOL ok = FALSE;
    if (len > 0) {
        ok = WriteFile(file, text, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);

    append_logf(
        "league news body file write source=%s title=%s id=%u ok=%u bytes=%lu path=%s",
        source != NULL ? source : "",
        title,
        message_id,
        ok ? 1u : 0u,
        written,
        path);
    return ok && written > 0;
}

static int assign_kbo_news_pointer_string(void* news_object, uint32_t offset, const char* text)
{
    if (news_object == NULL || text == NULL
            || !memory_range_readable((uint8_t*)news_object + offset, sizeof(void*))) {
        return 0;
    }

    void* string_object = *(void**)((uint8_t*)news_object + offset);
    if (string_object == NULL || !memory_range_readable(string_object, 0x18)) {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return 0;
    }

    OotpCoreStringAssignFn assign_string = (OotpCoreStringAssignFn)((uintptr_t)exe + OOTP27_PISD_STRING_ASSIGN_RVA);
    if (!memory_range_readable((void*)assign_string, 16)) {
        return 0;
    }

    assign_string(string_object, text);
    return 1;
}

static uint32_t kbo_read_news_u32(void* object, uint32_t offset)
{
    if (object == NULL || !memory_range_readable((uint8_t*)object + offset, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)((uint8_t*)object + offset);
}

static int create_kbo_real_add_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body,
    const char* source)
{
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t global = get_ootp_global_database();
    uintptr_t manager = global;
    if (exe == NULL || manager == 0 || title == NULL || title[0] == '\0') {
        append_logf(
            "league news real_add skipped source=%s title=%s reason=no_exe_manager_or_title exe=%p manager=%p global=%p",
            source != NULL ? source : "",
            title != NULL ? title : "",
            exe,
            (void*)manager,
            (void*)global);
        return 0;
    }

    OotpOperatorNewFn ootp_new = (OotpOperatorNewFn)((uintptr_t)exe + OOTP27_UI_OPERATOR_NEW_RVA);
    OotpNewsObjectCtorFn ctor = (OotpNewsObjectCtorFn)((uintptr_t)exe + OOTP27_NEWS_OBJECT_CTOR_RVA);
    OotpNewsStringEnsureFn ensure_strings = (OotpNewsStringEnsureFn)((uintptr_t)exe + OOTP27_NEWS_STRING_ENSURE_RVA);
    OotpLeagueNewsRealAddFn real_add = (OotpLeagueNewsRealAddFn)((uintptr_t)exe + OOTP27_LEAGUE_NEWS_REAL_ADD_RVA);
    if (!memory_range_readable((void*)ootp_new, 16)
            || !memory_range_readable((void*)ctor, 16)
            || !memory_range_readable((void*)ensure_strings, 16)
            || !memory_range_readable((void*)real_add, 16)) {
        append_logf(
            "league news real_add skipped source=%s title=%s reason=func_unreadable new=%p ctor=%p ensure=%p add=%p",
            source != NULL ? source : "",
            title,
            (void*)ootp_new,
            (void*)ctor,
            (void*)ensure_strings,
            (void*)real_add);
        return 0;
    }

    uint8_t* news = (uint8_t*)ootp_new(OOTP27_MESSAGE_OBJECT_SIZE);
    if (news == NULL || !memory_range_readable(news, OOTP27_MESSAGE_OBJECT_SIZE)) {
        append_logf(
            "league news real_add skipped source=%s title=%s reason=alloc_failed object=%p",
            source != NULL ? source : "",
            title,
            news);
        return 0;
    }

    ctor(news);
    ensure_strings(news);

    *(uint32_t*)(news + 0x44u) = league_id;
    *(uint32_t*)(news + 0x50u) = 3u;
    *(uint32_t*)(news + 0x54u) = 0u;
    *(uint32_t*)(news + 0x58u) = 1u;
    *(uint32_t*)(news + 0x5cu) = 3u;
    *(uint32_t*)(news + 0x60u) = league_id;
    *(uint32_t*)(news + 0x64u) = 0xffffffffu;
    *(uint32_t*)(news + 0x68u) = 0u;
    *(uint16_t*)(news + 0x78u) = (uint16_t)year;
    *(news + 0x7au) = (uint8_t)day;
    *(news + 0x7bu) = (uint8_t)month;
    *(uint32_t*)(news + 0x7cu) = 0u;
    *(uint32_t*)(news + 0x80u) = 0x00000100u;
    *(uint32_t*)(news + 0x84u) = 0xffffffffu;
    *(uint32_t*)(news + 0x88u) = 0x00000001u;
    *(uint32_t*)(news + OOTP27_MESSAGE_OBJECT_ID_OFFSET) = 0u;

    int title_ok = assign_kbo_news_pointer_string(news, 0x90u, title);
    int body_ok = assign_kbo_news_pointer_string(news, 0x98u, body != NULL ? body : "");
    uint32_t result = real_add((void*)manager, news, 0);
    uint32_t message_id = kbo_read_news_u32(news, OOTP27_MESSAGE_OBJECT_ID_OFFSET);
    int body_file_ok = 0;
    if (result != 0 && message_id != 0) {
        body_file_ok = write_kbo_message_body_file(message_id, title, body, source);
    }

    append_logf(
        "league news real_add create source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u manager=%p object=%p result=%u id8c=%u title_ok=%d body_ok=%d body_file=%d",
        source != NULL ? source : "",
        title,
        year,
        month,
        day,
        league_id,
        message_type,
        (void*)manager,
        news,
        result,
        message_id,
        title_ok,
        body_ok,
        body_file_ok);
    return result != 0;
}

static int kbo_current_date_is_valid(uint32_t* out_year, uint32_t* out_month, uint32_t* out_day)
{
    if (out_year != NULL) {
        *out_year = 0;
    }
    if (out_month != NULL) {
        *out_month = 0;
    }
    if (out_day != NULL) {
        *out_day = 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    if (current_date == 0 || !memory_range_readable((void*)current_date, OOTP27_CURRENT_DATE_DAY_OFFSET + sizeof(uint8_t))) {
        return 0;
    }

    uint32_t year  = *(uint16_t*)(current_date + OOTP27_CURRENT_DATE_YEAR_OFFSET);
    uint32_t month = *(uint8_t*)(current_date  + OOTP27_CURRENT_DATE_MONTH_OFFSET);
    uint32_t day   = *(uint8_t*)(current_date  + OOTP27_CURRENT_DATE_DAY_OFFSET);

    char scratch[16] = {0};
    if (!kbo_format_history_date(scratch, sizeof(scratch), year, month, day)) {
        return 0;
    }

    if (out_year  != NULL) { *out_year  = year;  }
    if (out_month != NULL) { *out_month = month; }
    if (out_day   != NULL) { *out_day   = day;   }
    return 1;
}

static int kbo_current_year_relaxed(uint32_t* out_year)
{
    if (out_year != NULL) {
        *out_year = 0;
    }

    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (kbo_current_date_is_valid(&year, &month, &day)) {
        if (out_year != NULL) {
            *out_year = year;
        }
        return 1;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    if (current_date == 0 || !memory_range_readable((void*)current_date, OOTP27_CURRENT_DATE_YEAR_OFFSET + sizeof(uint16_t))) {
        return 0;
    }

    year = *(uint16_t*)(current_date + OOTP27_CURRENT_DATE_YEAR_OFFSET);
    if (year < 1800 || year > 2300) {
        return 0;
    }

    if (out_year != NULL) {
        *out_year = year;
    }
    return 1;
}

static int kbo_current_history_date(char* out, size_t out_size, uint32_t fallback_year, const char* event_type)
{
    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
        if (current_date != 0
                && memory_range_readable((void*)current_date, OOTP27_CURRENT_DATE_DAY_OFFSET + sizeof(uint8_t))) {
            uint32_t year  = *(uint16_t*)(current_date + OOTP27_CURRENT_DATE_YEAR_OFFSET);
            uint32_t month = *(uint8_t*)(current_date  + OOTP27_CURRENT_DATE_MONTH_OFFSET);
            uint32_t day   = *(uint8_t*)(current_date  + OOTP27_CURRENT_DATE_DAY_OFFSET);
            if (kbo_format_history_date(out, out_size, year, month, day)) {
                return 1;
            }
        }
    }

    (void)event_type;
    return kbo_format_history_date(out, out_size, fallback_year, 1, 1);
}

static int read_kbo_localappdata_flag_file(const char* file_name)
{
    if (file_name == NULL || file_name[0] == '\0') {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\%s", local_app_data, file_name);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char buffer[16] = {0};
    DWORD read = 0;
    ReadFile(file, buffer, sizeof(buffer) - 1, &read, NULL);
    CloseHandle(file);
    return buffer[0] == '1' || buffer[0] == 'y' || buffer[0] == 'Y'
        || buffer[0] == 't' || buffer[0] == 'T';
}

static int kbo_fix_enabled(void)
{
    /* The DLL is purpose-injected by KBOLauncher, so it is enabled by default.
       Write enable_kbo_fix.txt with "0" to the OOTP-KBO folder to opt out. */
    static LONG cached = -1;
    LONG value = cached;
    if (value != -1) {
        return value == 1;
    }

    /* Opt-out check: if the file exists and starts with 0/n/N/f/F, disable. */
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    int enabled = 1;
    if (got > 0 && got < sizeof(local_app_data)) {
        char path[MAX_PATH] = {0};
        snprintf(path, sizeof(path), "%s\\OOTP-KBO\\enable_kbo_fix.txt", local_app_data);
        HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            char buf[16] = {0};
            DWORD rd = 0;
            ReadFile(f, buf, sizeof(buf) - 1, &rd, NULL);
            CloseHandle(f);
            if (buf[0] == '0' || buf[0] == 'n' || buf[0] == 'N' || buf[0] == 'f' || buf[0] == 'F') {
                enabled = 0;
            }
        }
    }

    InterlockedCompareExchange(&cached, enabled ? 1 : 0, -1);
    append_logf("KBO fix opt-in=%d", enabled ? 1 : 0);
    return enabled;
}

static uintptr_t get_kbo_league_event_manager(void)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t event_manager = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    if (event_manager == 0 || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    return event_manager;
}

static uint32_t kbo_read_kbo_league_id_override(void)
{
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        append_logf("KBO league id override: LOCALAPPDATA missing or invalid=%lu", got);
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\kbo_league_id.txt", local_app_data);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO league id override file missing=%s", path);
        return 0;
    }

    char raw[64] = {0};
    DWORD read = 0;
    ReadFile(file, raw, sizeof(raw) - 1, &read, NULL);
    CloseHandle(file);
    if (read == 0) {
        append_logf("KBO league id override read failed: empty file=%s", path);
        return 0;
    }

    raw[sizeof(raw) - 1] = '\0';

    uint32_t value = 0;
    size_t idx = 0;
    if (read >= 3u && (unsigned char)raw[0] == 0xEFu && (unsigned char)raw[1] == 0xBBu && (unsigned char)raw[2] == 0xBFu) {
        idx = 3u;
    }

    if (idx + 2u < (size_t)read && (unsigned char)raw[idx] == 0xFFu && (unsigned char)raw[idx + 1u] == 0xFEu) {
        idx += 2u;
        while (idx + 1u < (size_t)read) {
            uint16_t ch = (uint16_t)((unsigned char)raw[idx]) | ((uint16_t)(unsigned char)raw[idx + 1u] << 8);
            if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t') {
                idx += 2u;
                continue;
            }
            if (ch >= '0' && ch <= '9') {
                value = (uint32_t)(value * 10u + (unsigned)(ch - '0'));
                idx += 2u;
                while (idx + 1u < (size_t)read) {
                    ch = (uint16_t)((unsigned char)raw[idx]) | ((uint16_t)(unsigned char)raw[idx + 1u] << 8);
                    if (ch >= '0' && ch <= '9') {
                        value = (uint32_t)(value * 10u + (unsigned)(ch - '0'));
                        idx += 2u;
                        continue;
                    }
                    break;
                }
            }
            break;
        }
        if (value == 0) {
            append_logf("KBO league id override parse failed (utf16) file=%s read=%lu", path, read);
            return 0;
        }
        append_logf("KBO league id override file=%s utf16 value=%u", path, value);
        return value;
    }

    while (idx < (size_t)read && (raw[idx] == '\r' || raw[idx] == '\n' || raw[idx] == ' ' || raw[idx] == '\t')) {
        ++idx;
    }
    if (idx >= (size_t)read) {
        append_logf("KBO league id override parse failed: no body in file=%s", path);
        return 0;
    }

    while (idx < (size_t)read && raw[idx] >= '0' && raw[idx] <= '9') {
        value = (uint32_t)(value * 10u + (unsigned)(raw[idx] - '0'));
        ++idx;
    }
    if (value == 0) {
        append_logf("KBO league id override parse failed file=%s", path);
        return 0;
    }

    append_logf("KBO league id override file=%s value=%u", path, value);
    return value;
}
static uint32_t kbo_find_current_kbo_league_year(void)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    return kbo_current_date_is_valid(&year, &month, &day) ? year : 0;
}

static uint32_t kbo_find_league_year_from_id(uint32_t league_id)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u,
        0xe0u, 0xe8u, 0xf0u, 0xf8u, 0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };

    for (size_t i = 0; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        uint32_t vec_off = league_vec_offsets[i];
        uint32_t cnt_off = vec_off + 8u;
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }

        uintptr_t candidate_vec = *(uintptr_t*)(global + vec_off);
        int32_t candidate_count = *(int32_t*)(global + cnt_off);
        if (candidate_vec == 0 || candidate_count <= 0 || candidate_count > 10000
                || !memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }

        for (int32_t j = 0; j < candidate_count; j++) {
            uintptr_t league_candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            if (league_candidate == 0 || !memory_range_readable((void*)league_candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 4u)) {
                continue;
            }
            if (*(uint32_t*)(league_candidate + OOTP27_KBO_LEAGUE_ID_OFFSET) != league_id) {
                continue;
            }
            if (!memory_range_readable((void*)(league_candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
                return 0;
            }
            return *(uint32_t*)(league_candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        }
    }

    return 0;
}

static uint32_t kbo_resolve_kbo_league_id(void)
{
    static uint32_t cached_league_id = 0;
    uint32_t override = kbo_read_kbo_league_id_override();
    if (override != 0) {
        if (cached_league_id == 0 || cached_league_id != override) {
            append_logf("KBO league id override used override=%u cached=%u", override, cached_league_id);
        }
        cached_league_id = override;
        append_logf("KBO league id resolved by override=%u", cached_league_id);
        return cached_league_id;
    }

    if (cached_league_id != 0) {
        return cached_league_id;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    uint32_t league_ids[128] = {0};
    int32_t counts[128] = {0};
    int32_t seen = 0;

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        if (((uint8_t*)team_ptr)[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0 || league_id > 2000000u) {
            continue;
        }

        int index = -1;
        for (int j = 0; j < seen; j++) {
            if (league_ids[j] == league_id) {
                index = j;
                break;
            }
        }
        if (index < 0) {
            if (seen >= (int32_t)(sizeof(league_ids) / sizeof(league_ids[0]))) {
                continue;
            }
            league_ids[seen] = league_id;
            counts[seen] = 0;
            index = seen;
            seen++;
        }
        counts[index]++;
    }

    uint32_t current_year = kbo_find_current_kbo_league_year();
    uint32_t best_league_id = 0;
    int best_count = -1;
    for (int i = 0; i < seen; i++) {
        if (counts[i] <= 0) {
            continue;
        }

        uint32_t league_year = 0;
        if (current_year != 0) {
            league_year = kbo_find_league_year_from_id(league_ids[i]);
        }
        int score = counts[i];
        if (current_year != 0 && league_year == current_year) {
            score += 400;
        }
        if (score > best_count) {
            best_count = score;
            best_league_id = league_ids[i];
        }
    }

    if (best_league_id == 0 && seen > 0) {
        int fallback_count = counts[0];
        best_league_id = league_ids[0];
        for (int i = 1; i < seen; i++) {
            if (counts[i] > fallback_count) {
                fallback_count = counts[i];
                best_league_id = league_ids[i];
            }
        }
    }

    if (best_league_id == 0) {
        best_league_id = OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    cached_league_id = best_league_id;
    append_logf("KBO league id resolved current_year=%u team_count=%d selected=%u",
        current_year, team_count, best_league_id);
    return best_league_id;
}

static int kbo_league_event_exists(
    uintptr_t event_manager,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t event_type,
    const char* title)
{
    if (event_manager == 0 || title == NULL || title[0] == '\0') {
        return 0;
    }
    if (!memory_range_readable((void*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET), sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)event_type) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)year) {
            continue;
        }
        if (event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)month
                || event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)day) {
            continue;
        }

        char existing_title[128] = {0};
        if (!copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, existing_title, sizeof(existing_title))) {
            continue;
        }
        if (ascii_equals_ignore_case(existing_title, title)) {
            return 1;
        }
    }

    return 0;
}

static int create_kbo_league_event(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t event_type,
    const char* title,
    uint16_t aux_id,
    const char* source)
{
    if (title == NULL || title[0] == '\0' || league_id == 0 || year < 1800 || year > 2200
            || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_logf("league event create skipped source=%s title=%s reason=no_exe_module", source != NULL ? source : "", title);
        return 0;
    }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0) {
        append_logf(
            "league event create skipped source=%s title=%s reason=event_manager_unreadable",
            source != NULL ? source : "",
            title);
        return 0;
    }

    if (kbo_league_event_exists(event_manager, year, month, day, league_id, event_type, title)) {
        append_logf(
            "league event create skipped source=%s title=%s reason=already_exists date=%04u-%02u-%02u league_id=%u type=%u manager=%p",
            source != NULL ? source : "",
            title,
            year,
            month,
            day,
            league_id,
            event_type,
            (void*)event_manager);
        return 0;
    }

    OotpCreateLeagueEventFn create_event = (OotpCreateLeagueEventFn)((uintptr_t)exe + OOTP27_CREATE_LEAGUE_EVENT_RVA);
    if (!memory_range_readable((void*)create_event, 16)) {
        append_logf("league event create skipped source=%s title=%s reason=create_func_unreadable fn=%p", source != NULL ? source : "", title, (void*)create_event);
        return 0;
    }

    uint8_t ootp_date[16] = {0};
    *(uint16_t*)(ootp_date + 8) = (uint16_t)year;
    ootp_date[10] = (uint8_t)day;
    ootp_date[11] = (uint8_t)month;

    void* event = create_event(
        (void*)event_manager,
        ootp_date,
        event_type,
        league_id,
        title,
        aux_id);

    append_logf(
        "league event create source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u aux=%u manager=%p event=%p",
        source != NULL ? source : "",
        title,
        year,
        month,
        day,
        league_id,
        event_type,
        (uint32_t)aux_id,
        (void*)event_manager,
        event);
    append_logf(
        "league event verify source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u present=%d",
        source != NULL ? source : "",
        title,
        year,
        month,
        day,
        league_id,
        event_type,
        kbo_league_event_exists(event_manager, year, month, day, league_id, event_type, title));
    return event != NULL;
}

static int collect_kbo_league_team_ids(
    uint32_t league_id,
    uint32_t* team_ids,
    int max_team_ids,
    int* out_scanned,
    int* out_unreadable)
{
    if (out_scanned != NULL) {
        *out_scanned = 0;
    }
    if (out_unreadable != NULL) {
        *out_unreadable = 0;
    }
    if (league_id == 0 || team_ids == NULL || max_team_ids <= 0) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int found = 0;
    int unreadable = 0;
    for (int32_t i = 0; i < team_count && found < max_team_ids; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            unreadable++;
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id == 0 || team_id > 1000000u) {
            continue;
        }

        int duplicate = 0;
        for (int j = 0; j < found; j++) {
            if (team_ids[j] == team_id) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            team_ids[found++] = team_id;
        }
    }

    if (out_scanned != NULL) {
        *out_scanned = team_count;
    }
    if (out_unreadable != NULL) {
        *out_unreadable = unreadable;
    }
    return found;
}

static int create_kbo_core_message_news(uint32_t league_id, const char* title, const char* body, const char* source)
{
    if (title == NULL || title[0] == '\0') {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t global = get_ootp_global_database();
    if (exe == NULL || global == 0
            || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        append_logf("core message news skipped source=%s title=%s reason=no_global", source != NULL ? source : "", title);
        return 0;
    }

    uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    void* live_date = NULL;
    if (current_date != 0 && memory_range_readable((void*)(current_date + 0x1b0u), 16)) {
        live_date = (void*)(current_date + 0x1b0u);
    }
    if (live_date == NULL) {
        append_logf("core message news skipped source=%s title=%s reason=no_live_date", source != NULL ? source : "", title);
        return 0;
    }

    OotpCreateMessageCoreFn create_core = (OotpCreateMessageCoreFn)((uintptr_t)exe + OOTP27_CREATE_MESSAGE_CORE_RVA);
    if (!memory_range_readable((void*)create_core, 16)) {
        append_logf("core message news skipped source=%s title=%s reason=func_unreadable fn=%p", source != NULL ? source : "", title, (void*)create_core);
        return 0;
    }

    char live_text[1024] = {0};
    snprintf(live_text, sizeof(live_text), "%s: %s", title, body != NULL ? body : "");

    uint32_t team_ids[128] = {0};
    int scanned = 0;
    int unreadable = 0;
    int team_count = collect_kbo_league_team_ids(league_id, team_ids, 128, &scanned, &unreadable);
    int created = 0;
    for (int i = 0; i < team_count; i++) {
        create_core((void*)global, 10u, team_ids[i], live_date, live_text);
        created++;
    }
    if (created == 0) {
        create_core((void*)global, 10u, league_id, live_date, live_text);
        created = 1;
    }

    append_logf(
        "core message news fanout source=%s title=%s league_id=%u team_targets=%d scanned=%d unreadable=%d created=%d",
        source != NULL ? source : "",
        title,
        league_id,
        team_count,
        scanned,
        unreadable,
        created);
    return created > 0;
}

static int create_kbo_native_live_news_with_body(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body)
{
    if (title == NULL || title[0] == '\0') {
        return 0;
    }
    if (body == NULL || body[0] == '\0') {
        body = "";
    }

    int event_created = 0;
    int native_created = 0;
    append_logf(
        "native live news create_news_item skipped title=%s reason=unsafe_rva_call_disabled",
        title);

    int sql_created = 0;
    append_logf(
        "league news sql skipped source=%s title=%s reason=unsafe_sql_exec_disabled",
        "native_live_news",
        title);
    int real_created = 0;
    append_logf(
        "league news real_add skipped source=%s title=%s reason=unsafe_operator_new_rva_disabled",
        "native_live_news",
        title);
    int core_created = 0;
    append_logf(
        "core message news skipped source=%s title=%s reason=unsafe_rva_call_disabled",
        "native_live_news",
        title);
    append_logf(
        "native live news result title=%s date=%04u-%02u-%02u league_id=%u type=%u event=%d sql=%d real=%d native=%d core=%d",
        title,
        year,
        month,
        day,
        league_id,
        message_type,
        event_created,
        sql_created,
        real_created,
        native_created,
        core_created);
    return event_created || sql_created || real_created || native_created || core_created;
}

static int create_kbo_native_live_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title)
{
    return create_kbo_native_live_news_with_body(
        year,
        month,
        day,
        league_id,
        message_type,
        title,
        NULL);
}

static void flush_pending_special_player_history_sql(const char* source)
{
    (void)source;
}

static void append_special_player_history_csv(
    uint32_t player_id,
    uint16_t season,
    const char* history_date,
    const char* event_type,
    const char* history_text)
{
    (void)player_id;
    (void)season;
    (void)history_date;
    (void)event_type;
    (void)history_text;
}
