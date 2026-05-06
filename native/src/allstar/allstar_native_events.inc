/* Calls OOTP's native all-star event generator after KBO bootstrap is ready. */

typedef struct KboAllstarScheduleDate {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} KboAllstarScheduleDate;

static void log_kbo_allstar_native_event_state(const char* prefix, uintptr_t league_ptr, const char* source);

static int kbo_allstar_schedule_date_ready(uint8_t* league)
{
    if (league == NULL || !memory_range_readable(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET, 8u)) {
        return 0;
    }

    uint16_t year = *(uint16_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET);
    uint8_t day = *(uint8_t*)(league + OOTP27_ALLSTAR_DATE_DAY_OFFSET);
    uint8_t month = *(uint8_t*)(league + OOTP27_ALLSTAR_DATE_MONTH_OFFSET);
    return year >= 1982u && year <= 2200u && month >= 1u && month <= 12u && day >= 1u && day <= 31u;
}

static int kbo_allstar_season_start_date_ready(uint8_t* league)
{
    if (league == NULL || !memory_range_readable(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET, 8u)) {
        return 0;
    }

    uint16_t year = *(uint16_t*)(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint8_t day = *(uint8_t*)(league + OOTP27_SEASON_START_DATE_DAY_OFFSET);
    uint8_t month = *(uint8_t*)(league + OOTP27_SEASON_START_DATE_MONTH_OFFSET);
    return year >= 1982u && year <= 2200u && month >= 1u && month <= 12u && day >= 1u && day <= 31u;
}

static int kbo_allstar_read_uint_attr(const char* text, const char* name, uint32_t* out_value)
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
    while (*p == ' ' || *p == '\t' || *p == '\"' || *p == '\'') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    uint32_t value = 0;
    while (*p >= '0' && *p <= '9') {
        value = value * 10u + (uint32_t)(*p - '0');
        ++p;
    }

    *out_value = value;
    return 1;
}

static int kbo_allstar_serial_to_ymd(uint32_t serial, uint32_t* out_year, uint32_t* out_month, uint32_t* out_day)
{
    if (serial == 0 || out_year == NULL || out_month == NULL || out_day == NULL) {
        return 0;
    }

    uint32_t year = serial / 365u + 1u;
    while (year > 1u && kbo_date_serial(year, 1u, 1u) > serial) {
        --year;
    }
    while (kbo_date_serial(year + 1u, 1u, 1u) != 0u && kbo_date_serial(year + 1u, 1u, 1u) <= serial) {
        ++year;
    }

    uint32_t month = 1u;
    while (month < 12u) {
        uint32_t next_month_start = kbo_date_serial(year, month + 1u, 1u);
        if (next_month_start == 0u || next_month_start > serial) {
            break;
        }
        ++month;
    }

    uint32_t month_start = kbo_date_serial(year, month, 1u);
    if (month_start == 0u || serial < month_start) {
        return 0;
    }

    *out_year = year;
    *out_month = month;
    *out_day = serial - month_start + 1u;
    return 1;
}

static int kbo_allstar_try_load_schedule_file(
    const char* path,
    uint32_t league_year,
    KboAllstarScheduleDate* out_start,
    KboAllstarScheduleDate* out_allstar)
{
    if (path == NULL || out_start == NULL || out_allstar == NULL) {
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

    uint32_t start_month = 0;
    uint32_t start_day = 0;
    uint32_t allstar_day = 0;
    if (!kbo_allstar_read_uint_attr(text, "start_month", &start_month)
            || !kbo_allstar_read_uint_attr(text, "start_day", &start_day)
            || !kbo_allstar_read_uint_attr(text, "allstar_game_day", &allstar_day)) {
        return 0;
    }
    if (league_year < 1982u || league_year > 2200u
            || start_month < 1u || start_month > 12u
            || start_day < 1u || start_day > 31u
            || allstar_day < 1u || allstar_day > 250u
            || kbo_date_serial(league_year, start_month, start_day) == 0u) {
        return 0;
    }

    uint32_t allstar_serial = kbo_date_serial(league_year, start_month, start_day) + allstar_day - 1u;
    uint32_t allstar_year = 0;
    uint32_t allstar_month = 0;
    uint32_t allstar_dom = 0;
    if (!kbo_allstar_serial_to_ymd(allstar_serial, &allstar_year, &allstar_month, &allstar_dom)
            || allstar_year < 1982u || allstar_year > 2200u) {
        return 0;
    }

    uint32_t allstar_time = 1830u;
    const char* type4 = strstr(text, "type=\"4\"");
    if (type4 != NULL) {
        const char* line_start = type4;
        while (line_start > text && line_start[-1] != '\n' && line_start[-1] != '\r') {
            --line_start;
        }
        char line[512] = {0};
        size_t i = 0;
        while (line_start[i] != '\0' && line_start[i] != '\n' && line_start[i] != '\r' && i + 1u < sizeof(line)) {
            line[i] = line_start[i];
            ++i;
        }
        uint32_t parsed_time = 0;
        if (kbo_allstar_read_uint_attr(line, "time", &parsed_time) && parsed_time <= 2359u) {
            allstar_time = parsed_time;
        }
    }

    out_start->year = (uint16_t)league_year;
    out_start->month = (uint8_t)start_month;
    out_start->day = (uint8_t)start_day;
    out_start->hour = 0u;
    out_start->minute = 0u;
    out_start->second = 0u;

    out_allstar->year = (uint16_t)allstar_year;
    out_allstar->month = (uint8_t)allstar_month;
    out_allstar->day = (uint8_t)allstar_dom;
    out_allstar->hour = (uint8_t)(allstar_time / 100u);
    out_allstar->minute = (uint8_t)(allstar_time % 100u);
    out_allstar->second = 0u;
    return 1;
}

static int kbo_allstar_load_schedule_dates(uint32_t league_year, KboAllstarScheduleDate* out_start, KboAllstarScheduleDate* out_allstar, char* out_path, size_t out_path_size)
{
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
            league_year,
            suffixes[i]);
        if (written <= 0 || (size_t)written >= sizeof(path)) {
            continue;
        }
        if (!kbo_allstar_try_load_schedule_file(path, league_year, out_start, out_allstar)) {
            continue;
        }
        if (out_path != NULL && out_path_size > 0) {
            snprintf(out_path, out_path_size, "%s", path);
        }
        return 1;
    }

    return 0;
}

static int seed_kbo_allstar_schedule_dates(uintptr_t league_ptr, const char* source)
{
    uint8_t* league = (uint8_t*)league_ptr;
    if (league == NULL || !memory_range_readable(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET, 16u)) {
        return 0;
    }

    uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    KboAllstarScheduleDate start = {0};
    KboAllstarScheduleDate allstar = {0};
    char schedule_path[MAX_PATH] = {0};
    if (!kbo_allstar_load_schedule_dates(league_year, &start, &allstar, schedule_path, sizeof(schedule_path))) {
        append_logf(
            "KBO all-star native events date seed skipped source=%s league=%p year=%u reason=schedule_unavailable",
            source != NULL ? source : "",
            league,
            league_year);
        return 0;
    }

    if (!kbo_allstar_season_start_date_ready(league)) {
        *(uint16_t*)(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET) = start.year;
        league[OOTP27_SEASON_START_DATE_DAY_OFFSET] = start.day;
        league[OOTP27_SEASON_START_DATE_MONTH_OFFSET] = start.month;
        league[OOTP27_SEASON_START_DATE_HOUR_OFFSET] = start.hour;
        league[OOTP27_SEASON_START_DATE_MIN_OFFSET] = start.minute;
        league[OOTP27_SEASON_START_DATE_SEC_OFFSET] = start.second;
    }

    if (!kbo_allstar_schedule_date_ready(league)) {
        *(uint16_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET) = allstar.year;
        league[OOTP27_ALLSTAR_DATE_DAY_OFFSET] = allstar.day;
        league[OOTP27_ALLSTAR_DATE_MONTH_OFFSET] = allstar.month;
        league[OOTP27_ALLSTAR_DATE_HOUR_OFFSET] = allstar.hour;
        league[OOTP27_ALLSTAR_DATE_MIN_OFFSET] = allstar.minute;
        league[OOTP27_ALLSTAR_DATE_SEC_OFFSET] = allstar.second;
    }

    append_logf(
        "KBO all-star schedule dates seeded source=%s league=%p start=%04u-%02u-%02u allstar=%04u-%02u-%02u %02u:%02u path=%s",
        source != NULL ? source : "",
        league,
        (unsigned)start.year,
        (unsigned)start.month,
        (unsigned)start.day,
        (unsigned)allstar.year,
        (unsigned)allstar.month,
        (unsigned)allstar.day,
        (unsigned)allstar.hour,
        (unsigned)allstar.minute,
        schedule_path);
    return 1;
}

__declspec(noinline) void ootp_kbo_capture_allstar_schedule_import_league(uintptr_t league_ptr)
{
    if (league_ptr == 0 || !kbo_allstar_league_core_plausible(league_ptr)) {
        return;
    }

    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        return;
    }

    InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)league_ptr);
    enable_kbo_allstar_flags(league_ptr, "schedule_import");
    seed_kbo_allstar_schedule_dates(league_ptr, "schedule_import");

    static volatile LONG log_count = 0;
    LONG index = InterlockedIncrement(&log_count);
    if (index <= 12) {
        log_kbo_allstar_native_event_state("schedule_import_capture", league_ptr, "schedule_import");
    }
}

static void log_kbo_allstar_native_event_state(const char* prefix, uintptr_t league_ptr, const char* source)
{
    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (league == NULL || !memory_range_readable(league, layout.league_id_fallback_offset + sizeof(uint32_t))) {
        append_logf("KBO all-star native events %s source=%s league=%p reason=unreadable", prefix, source != NULL ? source : "", league);
        return;
    }

    uint16_t asg_year = 0;
    uint8_t asg_day = 0;
    uint8_t asg_month = 0;
    if (memory_range_readable(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET, 8u)) {
        asg_year = *(uint16_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET);
        asg_day = *(uint8_t*)(league + OOTP27_ALLSTAR_DATE_DAY_OFFSET);
        asg_month = *(uint8_t*)(league + OOTP27_ALLSTAR_DATE_MONTH_OFFSET);
    }

    append_logf(
        "KBO all-star native events %s source=%s league=%p league_id=%u/%u year=%u phase=%u game=%u auto=%u team0=0x%08x team1=0x%08x asg=%04u-%02u-%02u",
        prefix,
        source != NULL ? source : "",
        league,
        kbo_allstar_read_u32(league, layout.league_id_primary_offset),
        kbo_allstar_read_u32(league, layout.league_id_fallback_offset),
        *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET),
        *(uint8_t*)(league + OOTP27_KBO_LEAGUE_PHASE_OFFSET),
        *(uint8_t*)(league + layout.game_flag_offset),
        *(uint8_t*)(league + layout.auto_schedule_offset),
        kbo_allstar_read_u32(league, layout.team_a_offset),
        kbo_allstar_read_u32(league, layout.team_b_offset),
        (unsigned)asg_year,
        (unsigned)asg_month,
        (unsigned)asg_day);
}

static int run_kbo_allstar_native_event_generation(uintptr_t league_ptr, const char* source)
{
    if (InterlockedCompareExchange(&g_allstar_native_event_generation_done, 0, 0) != 0) {
        return 1;
    }
    if (InterlockedCompareExchange(&g_allstar_native_event_generation_in_progress, 1, 0) != 0) {
        return 0;
    }

    int generated = 0;
    do {
        if (!kbo_allstar_league_context_enabled(league_ptr)) {
            break;
        }

        KboAllstarLayout layout = kbo_get_allstar_layout();
        uint8_t* league = (uint8_t*)league_ptr;
        if (!memory_range_readable(league, layout.league_id_fallback_offset + sizeof(uint32_t))) {
            append_logf("KBO all-star native events skipped source=%s league=%p reason=league_unreadable", source != NULL ? source : "", league);
            break;
        }

        uintptr_t imported_league_ptr = (uintptr_t)InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
            NULL,
            NULL);
        if (imported_league_ptr != league_ptr) {
            append_logf(
                "KBO all-star native events skipped source=%s league=%p imported=%p reason=not_schedule_import_league",
                source != NULL ? source : "",
                league,
                (void*)imported_league_ptr);
            break;
        }

        enable_kbo_allstar_flags(league_ptr, source != NULL ? source : "native_allstar_events");
        if ((!kbo_allstar_season_start_date_ready(league) || !kbo_allstar_schedule_date_ready(league))
                && !seed_kbo_allstar_schedule_dates(league_ptr, source)) {
            log_kbo_allstar_native_event_state("waiting_date", league_ptr, source);
            break;
        }
        if (!kbo_allstar_season_start_date_ready(league) || !kbo_allstar_schedule_date_ready(league)) {
            log_kbo_allstar_native_event_state("waiting_date", league_ptr, source);
            break;
        }

        HMODULE exe = GetModuleHandleA(NULL);
        if (exe == NULL) {
            append_log_line("KBO all-star native events skipped: GetModuleHandleA(NULL) failed");
            break;
        }

        OotpMakeAllstarGameEventsFn make_events =
            (OotpMakeAllstarGameEventsFn)((uintptr_t)exe + OOTP27_MAKE_ALLSTAR_GAME_EVENTS_RVA);
        if (!memory_range_readable((void*)make_events, 32u)) {
            append_logf("KBO all-star native events skipped source=%s target=%p reason=target_unreadable", source != NULL ? source : "", (void*)make_events);
            break;
        }
        if (!kbo_allstar_league_vtable_plausible(league_ptr)) {
            append_logf("KBO all-star native events skipped source=%s league=%p reason=league_vtable_invalid", source != NULL ? source : "", league);
            break;
        }

        log_kbo_allstar_native_event_state("before_call", league_ptr, source);
        make_events(league_ptr, 1);
        log_kbo_allstar_native_event_state("after_call", league_ptr, source);

        InterlockedExchange(&g_allstar_native_event_generation_done, 1);
        generated = 1;
    } while (0);

    InterlockedExchange(&g_allstar_native_event_generation_in_progress, 0);
    return generated;
}
