static int kbo_amateur_assignment_debug_csv_empty(HANDLE file)
{
    LARGE_INTEGER size;
    if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &size)) {
        return 1;
    }
    return size.QuadPart == 0;
}

static void kbo_amateur_assignment_debug_write_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

static void kbo_amateur_assignment_debug_write_csv_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    kbo_amateur_assignment_debug_write_text(file, "\"");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                kbo_amateur_assignment_debug_write_text(file, "\"\"");
            } else {
                char ch[2] = { *p, '\0' };
                kbo_amateur_assignment_debug_write_text(file, ch);
            }
        }
    }
    kbo_amateur_assignment_debug_write_text(file, "\"");
}

static void kbo_amateur_assignment_append_debug_csv(
    const char* phase,
    const char* source,
    uint32_t player_id,
    uint32_t league_id,
    int16_t age,
    int32_t quality_score,
    int player_tier,
    uint32_t from_team_id,
    uint8_t from_reputation,
    int from_tier,
    int32_t from_player_count,
    uint32_t to_team_id,
    uint8_t to_reputation,
    int to_tier,
    int32_t to_player_count,
    int32_t target_player_count,
    uint8_t target_reputation,
    int original_result,
    uint32_t after_team_id,
    uint32_t after_league_id)
{
    (void)phase;
    (void)source;
    (void)player_id;
    (void)league_id;
    (void)age;
    (void)quality_score;
    (void)player_tier;
    (void)from_team_id;
    (void)from_reputation;
    (void)from_tier;
    (void)from_player_count;
    (void)to_team_id;
    (void)to_reputation;
    (void)to_tier;
    (void)to_player_count;
    (void)target_player_count;
    (void)target_reputation;
    (void)original_result;
    (void)after_team_id;
    (void)after_league_id;
    return;

    if (strcmp(phase != NULL ? phase : "", "original_success") == 0) {
        static volatile LONG success_csv_count = 0;
        if (InterlockedIncrement(&success_csv_count) > 300) {
            return;
        }
    }
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_file("amateur_assignment_debug.csv", path, sizeof(path))) {
        return;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        static volatile LONG open_log_count = 0;
        if (InterlockedIncrement(&open_log_count) <= 5) {
            append_logf("amateur assignment debug csv open failed path=%s gle=%lu", path, GetLastError());
        }
        return;
    }

    if (kbo_amateur_assignment_debug_csv_empty(file)) {
        kbo_amateur_assignment_debug_write_text(
            file,
            "year,month,day,phase,source,player_id,league_id,age,quality_score,player_tier,"
            "from_team_id,from_reputation,from_tier,from_player_count,"
            "to_team_id,to_reputation,to_tier,to_player_count,target_player_count,target_reputation,"
            "original_result,after_team_id,after_league_id\r\n");
    }

    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    kbo_current_date_is_valid(&year, &month, &day);

    char line[1024] = {0};
    snprintf(
        line,
        sizeof(line),
        "%u,%u,%u,",
        year,
        month,
        day);
    kbo_amateur_assignment_debug_write_text(file, line);
    kbo_amateur_assignment_debug_write_csv_text(file, phase != NULL ? phase : "");
    kbo_amateur_assignment_debug_write_text(file, ",");
    kbo_amateur_assignment_debug_write_csv_text(file, source != NULL ? source : "");
    snprintf(
        line,
        sizeof(line),
        ",%u,%u,%d,%d,%d,%u,%u,%d,%d,%u,%u,%d,%d,%d,%u,%d,%u,%u\r\n",
        player_id,
        league_id,
        (int)age,
        quality_score,
        player_tier,
        from_team_id,
        (uint32_t)from_reputation,
        from_tier,
        from_player_count,
        to_team_id,
        (uint32_t)to_reputation,
        to_tier,
        to_player_count,
        target_player_count,
        (uint32_t)target_reputation,
        original_result,
        after_team_id,
        after_league_id);
    kbo_amateur_assignment_debug_write_text(file, line);
    CloseHandle(file);
}
