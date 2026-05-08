static int kbo_fa_salary_snapshot_path(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0 || season < 1982u || season > 2200u) {
        return 0;
    }

    char file_name[96] = {0};
    snprintf(file_name, sizeof(file_name), "fa_salary_opening_day_snapshot_%04u.csv", season);
    return kbo_get_save_scoped_data_file(file_name, out, out_size);
}

static int kbo_fa_salary_snapshot_file_exists(uint32_t season)
{
    LONG cached_season = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, 0, 0);
    LONG cached_value = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, -1, -1);
    if (cached_season == (LONG)season && cached_value != -1) {
        return cached_value == 1;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_salary_snapshot_path(season, path, sizeof(path))) {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, (LONG)season);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, 0);
        return 0;
    }

    int exists = attrs.nFileSizeHigh != 0u || attrs.nFileSizeLow != 0u;
    InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, (LONG)season);
    InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, exists ? 1 : 0);
    return exists;
}

static uint32_t kbo_fa_salary_snapshot_team_league_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }

    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

static uint32_t kbo_fa_salary_snapshot_resolve_ranking_team(uint32_t league_id, uint32_t current_team_id, uint32_t active_team_id)
{
    if (active_team_id != 0u && kbo_fa_salary_snapshot_team_league_id(active_team_id) == league_id) {
        return active_team_id;
    }
    if (current_team_id != 0u && kbo_fa_salary_snapshot_team_league_id(current_team_id) == league_id) {
        return current_team_id;
    }
    return 0u;
}

static int kbo_fa_salary_snapshot_read_opening_day(uintptr_t league_ptr, uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }
    if (league_ptr == 0
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET),
                OOTP27_SEASON_START_DATE_SEC_OFFSET - OOTP27_SEASON_START_DATE_YEAR_OFFSET + sizeof(uint8_t))) {
        return 0;
    }

    uint32_t year = *(uint16_t*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint32_t day = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_DAY_OFFSET);
    uint32_t month = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_MONTH_OFFSET);
    if (year < 1982u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    char scratch[16] = {0};
    if (!kbo_format_history_date(scratch, sizeof(scratch), year, month, day)) {
        return 0;
    }

    if (out_date != NULL) {
        *out_date = year * 10000u + month * 100u + day;
    }
    return 1;
}

static int kbo_fa_salary_snapshot_current_date_in_opening_window(uint32_t date, uint32_t opening_day)
{
    if (date == 0u || opening_day == 0u) {
        return 0;
    }

    uint32_t year = date / 10000u;
    uint32_t month = (date / 100u) % 100u;
    uint32_t day = date % 100u;
    uint32_t opening_year = opening_day / 10000u;
    uint32_t opening_month = (opening_day / 100u) % 100u;
    uint32_t opening_day_of_month = opening_day % 100u;
    uint32_t current_serial = kbo_date_serial(year, month, day);
    uint32_t opening_serial = kbo_date_serial(opening_year, opening_month, opening_day_of_month);
    if (current_serial == 0u || opening_serial == 0u || current_serial < opening_serial) {
        return 0;
    }
    return current_serial - opening_serial <= KBO_FA_SALARY_SNAPSHOT_OPENING_WINDOW_DAYS;
}

static int kbo_fa_salary_snapshot_read_uint_attr(const char* text, const char* name, uint32_t* out_value)
{
    if (text == NULL || name == NULL || out_value == NULL) {
        return 0;
    }

    const char* found = strstr(text, name);
    if (found == NULL) {
        return 0;
    }

    const char* equals = strchr(found, '=');
    if (equals == NULL) {
        return 0;
    }

    const char* p = equals + 1;
    while (*p == ' ' || *p == '\t' || *p == '"' || *p == '\'') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    uint32_t value = 0u;
    while (*p >= '0' && *p <= '9') {
        value = value * 10u + (uint32_t)(*p - '0');
        ++p;
    }

    *out_value = value;
    return 1;
}

static int kbo_fa_salary_snapshot_load_schedule_opening_day_from_file(const char* path, uint32_t season, uint32_t* out_opening_day)
{
    if (path == NULL || out_opening_day == NULL || season < 1982u || season > 2200u) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char text[8192] = {0};
    DWORD read = 0;
    BOOL ok = ReadFile(file, text, sizeof(text) - 1u, &read, NULL);
    CloseHandle(file);
    if (!ok || read == 0) {
        return 0;
    }
    text[read < sizeof(text) ? read : sizeof(text) - 1u] = '\0';

    uint32_t start_month = 0u;
    uint32_t start_day = 0u;
    if (!kbo_fa_salary_snapshot_read_uint_attr(text, "start_month", &start_month)
            || !kbo_fa_salary_snapshot_read_uint_attr(text, "start_day", &start_day)) {
        return 0;
    }
    if (start_month < 1u || start_month > 12u || start_day < 1u || start_day > 31u
            || kbo_date_serial(season, start_month, start_day) == 0u) {
        return 0;
    }

    *out_opening_day = season * 10000u + start_month * 100u + start_day;
    return 1;
}

static int kbo_fa_salary_snapshot_load_schedule_opening_day(uint32_t season, uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_opening_day == NULL || season < 1982u || season > 2200u) {
        return 0;
    }

    char exe_path[MAX_PATH] = {0};
    DWORD got = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (got == 0 || got >= sizeof(exe_path)) {
        return 0;
    }

    char* slash = strrchr(exe_path, '\\');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0';

    const char* suffixes[] = { "", "_ap" };
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        char path[MAX_PATH] = {0};
        int written = snprintf(
            path,
            sizeof(path),
            "%s\\data\\schedules\\major_league_ml_c_%u%s.lsdl",
            exe_path,
            season,
            suffixes[i]);
        if (written <= 0 || (size_t)written >= sizeof(path)) {
            continue;
        }
        if (kbo_fa_salary_snapshot_load_schedule_opening_day_from_file(path, season, out_opening_day)) {
            return 1;
        }
    }

    return 0;
}

static int kbo_fa_salary_snapshot_file_contains_opening_day_message(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 16384u) {
        CloseHandle(file);
        return 0;
    }

    char* data = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (data == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    BOOL ok = ReadFile(file, data, size, &read, NULL);
    CloseHandle(file);
    int found = ok
        && read > 0
        && strstr(data, "Breaking KBO News: Opening Day") != NULL
        && strstr(data, "Korean Baseball Organization") != NULL;
    HeapFree(GetProcessHeap(), 0, data);
    return found;
}

static int kbo_fa_salary_snapshot_today_has_opening_day_message(uint32_t date)
{
    if (date == 0u) {
        return 0;
    }

    uint32_t month = (date / 100u) % 100u;
    uint32_t day = date % 100u;
    if (month < 3u || month > 4u || day < 1u || day > 31u) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char pattern[MAX_PATH] = {0};
    snprintf(pattern, sizeof(pattern), "%s\\messages\\message*.txt", save_path);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int found = 0;
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        char path[MAX_PATH] = {0};
        snprintf(path, sizeof(path), "%s\\messages\\%s", save_path, find_data.cFileName);
        if (kbo_fa_salary_snapshot_file_contains_opening_day_message(path)) {
            found = 1;
            break;
        }
    } while (FindNextFileA(find, &find_data));

    FindClose(find);
    return found;
}

