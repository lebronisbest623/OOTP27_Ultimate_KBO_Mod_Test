/* Foreign waiver negotiation-window events and date helpers. Included from native/src/foreign_waiver_ai.inc. */

/* ---- Korean foreign waiver negotiation window control ----
 * Preferred mode: custom league events open and close the window.
 * File mode is fallback/override: if foreign_waiver_negotiation_window.txt exists
 * with valid YYYYMMDD,YYYYMMDD, that exact window is used.
 * The old season-end event scanner is opt-in only; award voting screens call the
 * cheap window check often, so scanning league events there is too expensive.
 */
static void kbo_queue_foreign_priority_league_event(
    uint32_t event_yyyymmdd,
    const char* title,
    const char* source)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_priority_pending_lock, 1, 0) != 0) {
        Sleep(0);
    }

    g_kbo_foreign_priority_pending_yyyymmdd = event_yyyymmdd;
    snprintf(
        g_kbo_foreign_priority_pending_title,
        sizeof(g_kbo_foreign_priority_pending_title),
        "%s",
        title != NULL ? title : "");
    snprintf(
        g_kbo_foreign_priority_pending_source,
        sizeof(g_kbo_foreign_priority_pending_source),
        "%s",
        source != NULL ? source : "foreign_priority_negotiation");

    InterlockedExchange(&g_kbo_foreign_priority_pending_lock, 0);
    append_logf(
        "foreign priority negotiation: league event queued source=%s title=%s date=%u",
        g_kbo_foreign_priority_pending_source,
        g_kbo_foreign_priority_pending_title,
        event_yyyymmdd);
}

static void kbo_flush_pending_foreign_priority_events(const char* source)
{
    uint32_t event_yyyymmdd = 0;
    char title[96] = {0};
    char queued_source[48] = {0};

    while (InterlockedCompareExchange(&g_kbo_foreign_priority_pending_lock, 1, 0) != 0) {
        Sleep(0);
    }

    event_yyyymmdd = g_kbo_foreign_priority_pending_yyyymmdd;
    if (event_yyyymmdd != 0u) {
        snprintf(title, sizeof(title), "%s", g_kbo_foreign_priority_pending_title);
        snprintf(queued_source, sizeof(queued_source), "%s", g_kbo_foreign_priority_pending_source);
        g_kbo_foreign_priority_pending_yyyymmdd = 0;
        g_kbo_foreign_priority_pending_title[0] = '\0';
        g_kbo_foreign_priority_pending_source[0] = '\0';
    }

    InterlockedExchange(&g_kbo_foreign_priority_pending_lock, 0);

    if (event_yyyymmdd == 0u || title[0] == '\0') {
        return;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (year < 1800u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        append_logf(
            "foreign priority negotiation: pending league event dropped source=%s title=%s date=%u reason=invalid_date",
            queued_source,
            title,
            event_yyyymmdd);
        return;
    }

    int created = create_kbo_league_event(
        year,
        month,
        day,
        kbo_get_foreign_waiver_league_id(),
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        0,
        queued_source[0] != '\0' ? queued_source : source);

    append_logf(
        "foreign priority negotiation: pending league event flushed source=%s flush_source=%s title=%s date=%u created=%d",
        queued_source,
        source != NULL ? source : "",
        title,
        event_yyyymmdd,
        created);
}

static int kbo_emit_foreign_priority_league_event(
    uint32_t event_yyyymmdd,
    const char* title,
    const char* source)
{
    if (event_yyyymmdd == 0u || title == NULL || title[0] == '\0') {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (year < 1800u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    if (create_kbo_league_event(
            year,
            month,
            day,
            kbo_get_foreign_waiver_league_id(),
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            title,
            0,
            source != NULL ? source : "foreign_priority_negotiation")) {
        append_logf(
            "foreign priority negotiation: league event created immediately source=%s title=%s date=%u",
            source != NULL ? source : "",
            title,
            event_yyyymmdd);
        return 1;
    }

    kbo_queue_foreign_priority_league_event(event_yyyymmdd, title, source);
    return 1;
}

static uint32_t kbo_detect_offseason_starts_event(uint32_t today_yyyymmdd, uint32_t league_id)
{
    static LONG debug_log_count = 0;
    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    uint32_t today_key = today_yyyymmdd;
    LONG debug_slot = InterlockedIncrement(&debug_log_count);
    int debug_remaining = (debug_slot <= 6 && read_kbo_localappdata_flag_file("enable_foreign_waiver_event_probe.txt")) ? 12 : 0;
    uint32_t latest_offseason_start = 0u;

    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (league_id != 0u && *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_league_id = *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET);
        if (event_year < 1980u || event_year > 2200u || event_month < 1u || event_month > 12u || event_day < 1u || event_day > 31u) {
            continue;
        }

        uint32_t event_key = event_year * 10000u + event_month * 100u + event_day;
        if (event_key > today_key) {
            continue;
        }

        char name[128] = {0};
        if (!copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }
        if (debug_remaining > 0
                && event_month >= 9u
                && event_month <= 12u) {
            append_logf(
                "foreign waiver auto: event probe idx=%d date=%04u-%02u-%02u league=%u type=%u over=%u name=%s event=%p",
                i,
                event_year,
                event_month,
                event_day,
                event_league_id,
                (uint32_t)(*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET)),
                (uint32_t)(*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET)),
                name,
                (void*)event_ptr);
            debug_remaining--;
        }
        if (_stricmp(name, "Offseason starts") == 0) {
            if (event_key > latest_offseason_start) {
                latest_offseason_start = event_key;
            }
        }
    }

    if (latest_offseason_start != 0u) {
        append_logf(
            "foreign waiver auto: detected latest Offseason starts date=%u (today=%u)",
            latest_offseason_start,
            today_yyyymmdd);
    }

    return latest_offseason_start;
}

static int kbo_days_in_month(uint32_t year, uint32_t month)
{
    static const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1u || month > 12u) {
        return 0;
    }
    if (month == 2u && kbo_is_leap_year(year)) {
        return 29;
    }
    return days[month];
}

static uint32_t kbo_add_one_month_yyyymmdd(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1980u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }
    month++;
    if (month > 12u) {
        month = 1u;
        year++;
    }
    int dim = kbo_days_in_month(year, month);
    if (dim <= 0) {
        return 0;
    }
    if (day > (uint32_t)dim) {
        day = (uint32_t)dim;
    }
    return year * 10000u + month * 100u + day;
}

static uint32_t kbo_add_days_yyyymmdd(uint32_t yyyymmdd, uint32_t add_days)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1980u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    while (add_days > 0u) {
        int dim = kbo_days_in_month(year, month);
        if (dim <= 0) {
            return 0;
        }
        day++;
        if (day > (uint32_t)dim) {
            day = 1u;
            month++;
            if (month > 12u) {
                month = 1u;
                year++;
            }
        }
        add_days--;
    }
    return year * 10000u + month * 100u + day;
}

static int kbo_write_foreign_waiver_window(uint32_t start_yyyymmdd, uint32_t end_yyyymmdd, const char* reason)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_event_path(path, sizeof(path))) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign priority negotiation: failed to write window file error=%lu", GetLastError());
        return 0;
    }

    char line[128] = {0};
    int len = snprintf(line, sizeof(line), "%08u,%08u\r\n", start_yyyymmdd, end_yyyymmdd);
    DWORD written = 0;
    int ok = WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len;
    CloseHandle(file);

    append_logf(
        "foreign priority negotiation: event window persisted start=%u end=%u reason=%s ok=%d path=%s",
        start_yyyymmdd,
        end_yyyymmdd,
        reason == NULL ? "" : reason,
        ok,
        path);
    return ok;
}

static int kbo_open_foreign_waiver_window(uint32_t today_yyyymmdd, uint32_t today_serial, const char* reason)
{
    uint32_t end_yyyymmdd = kbo_add_days_yyyymmdd(today_yyyymmdd, 20u);
    if (end_yyyymmdd == 0u) {
        return 0;
    }

    g_kbo_foreign_waiver_window_start_serial = today_serial;
    g_kbo_foreign_waiver_window_end_serial = today_serial + 20u;
    kbo_write_foreign_waiver_window(today_yyyymmdd, end_yyyymmdd, reason);
    g_kbo_foreign_waiver_start_event_date = today_yyyymmdd;
    append_logf(
        "foreign priority negotiation: season-end event opened start=%u end=%u serial=%u~%u reason=%s",
        today_yyyymmdd,
        end_yyyymmdd,
        g_kbo_foreign_waiver_window_start_serial,
        g_kbo_foreign_waiver_window_end_serial,
        reason == NULL ? "" : reason);
    return 1;
}

static int kbo_advance_foreign_waiver_window(uint32_t today_yyyymmdd, uint32_t today_serial)
{
    static uint32_t last_checked_yyyymmdd = 0u;
    static uint32_t last_checked_result = 0u;
    if (today_yyyymmdd != 0u
            && today_yyyymmdd == last_checked_yyyymmdd
            && last_checked_result != 0u) {
        return 0;
    }
    last_checked_yyyymmdd = today_yyyymmdd;
    last_checked_result = 0u;

    append_logf(
        "foreign waiver auto: evaluate season-end check today=%u serial=%u prev_marker=%u",
        today_yyyymmdd, today_serial,
        g_kbo_foreign_waiver_last_seen_yyyymmdd);

    if (today_yyyymmdd == 0 || today_serial == 0) {
        append_log_line("foreign waiver auto: season-end check skipped - invalid date");
        return 0;
    }

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        append_log_line("foreign waiver auto: missing configured league id; skip season-end evaluation");
        return 0;
    }

    uint32_t season_end_yyyymmdd = kbo_detect_offseason_starts_event(today_yyyymmdd, configured_league_id);
    if (season_end_yyyymmdd == 0u) {
        append_logf("foreign waiver auto: no Offseason starts event found today=%u", today_yyyymmdd);
        last_checked_result = 2u;
        return 0;
    }
    if (today_yyyymmdd < season_end_yyyymmdd) {
        append_logf(
            "foreign waiver auto: waiting for Offseason starts marker season_end=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        last_checked_result = 2u;
        return 0;
    }
    if (g_kbo_foreign_waiver_last_seen_yyyymmdd == season_end_yyyymmdd) {
        append_logf(
            "foreign waiver auto: already opened for marker=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        last_checked_result = 2u;
        return 0;
    }

    g_kbo_foreign_waiver_last_seen_yyyymmdd = season_end_yyyymmdd;
    append_logf(
        "foreign waiver auto: season-end marker confirmed season_end=%u today=%u",
        season_end_yyyymmdd,
        today_yyyymmdd);
    int opened = kbo_open_foreign_waiver_window(
        today_yyyymmdd,
        today_serial,
        "offseason_starts_event");
    last_checked_result = opened ? 1u : 2u;
    return opened;
}

static int kbo_parse_yyyymmdd(const char* date_text, uint32_t* out_date)
{
    if (date_text == NULL || out_date == NULL) {
        return 0;
    }

    char ymd[9] = {0};
    size_t len = strlen(date_text);
    if (len >= 8
            && date_text[0] >= '0' && date_text[0] <= '9'
            && date_text[1] >= '0' && date_text[1] <= '9'
            && date_text[2] >= '0' && date_text[2] <= '9'
            && date_text[3] >= '0' && date_text[3] <= '9'
            && date_text[4] >= '0' && date_text[4] <= '9'
            && date_text[5] >= '0' && date_text[5] <= '9'
            && date_text[6] >= '0' && date_text[6] <= '9'
            && date_text[7] >= '0' && date_text[7] <= '9') {
        memcpy(ymd, date_text, 8);
    } else if (len >= 10 && date_text[4] == '-' && date_text[7] == '-') {
        ymd[0] = date_text[0]; ymd[1] = date_text[1]; ymd[2] = date_text[2]; ymd[3] = date_text[3];
        ymd[4] = date_text[5]; ymd[5] = date_text[6];
        ymd[6] = date_text[8]; ymd[7] = date_text[9];
    } else {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        if (ymd[i] < '0' || ymd[i] > '9') {
            return 0;
        }
    }

    *out_date = (uint32_t)(
        (ymd[0] - '0') * 10000000u +
        (ymd[1] - '0') * 1000000u +
        (ymd[2] - '0') * 100000u +
        (ymd[3] - '0') * 10000u +
        (ymd[4] - '0') * 1000u +
        (ymd[5] - '0') * 100u +
        (ymd[6] - '0') * 10u +
        (ymd[7] - '0'));
    return 1;
}

static int kbo_get_current_yyyymmdd(uint32_t* out_date)
{
    if (out_date == NULL) {
        return 0;
    }

    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return 0;
    }
    *out_date = year * 10000u + month * 100u + day;
    return 1;
}

static int kbo_get_foreign_waiver_rights_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_rights.csv", out, out_size);
}

static int get_kbo_foreign_waiver_decisions_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_decisions.csv", out, out_size);
}

static int get_kbo_asian_quota_nation_ids_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\asian_quota_nation_ids.txt", local_app_data);
    return 1;
}

static int kbo_foreign_waiver_legacy_auto_detector_enabled(void)
{
    static LONG cached = -1;
    LONG value = cached;
    if (value != -1) {
        return value == 1;
    }
    value = read_kbo_localappdata_flag_file("enable_foreign_waiver_legacy_auto_detector.txt") ? 1 : 0;
    InterlockedCompareExchange(&cached, value, -1);
    return cached == 1;
}

static int get_kbo_foreign_waiver_announcement_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_announcements.txt", out, out_size);
}

static int kbo_foreign_waiver_announcement_recorded(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char raw[4096] = {0};
    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, raw, sizeof(raw) - 1u, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        const char* cursor = raw;
        while (*cursor != '\0') {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n' || *cursor == ',') {
                cursor++;
            }
            char* tail = NULL;
            unsigned long raw_date = strtoul(cursor, &tail, 10);
            if (raw_date == event_yyyymmdd) {
                found = 1;
                break;
            }
            if (tail == cursor) {
                break;
            }
            cursor = tail;
        }
    }

    CloseHandle(file);
    return found;
}

static void kbo_record_foreign_waiver_announcement(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char line[32] = {0};
    int len = snprintf(line, sizeof(line), "%u\r\n", event_yyyymmdd);
    DWORD written = 0;
    if (len > 0) {
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);
}

static void kbo_record_foreign_waiver_announcement_body(uint32_t event_yyyymmdd, const char* source, const char* body)
{
    if (event_yyyymmdd == 0u) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign reserve rights: announcement body record failed path=%s err=%lu", path, GetLastError());
        return;
    }

    char line[1400] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%u|%s|%s\r\n",
        event_yyyymmdd,
        source != NULL ? source : "",
        body != NULL ? body : "");
    DWORD written = 0;
    if (len > 0) {
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);
}

static int kbo_get_foreign_waiver_current_yyyymmdd(uint32_t* out_date)
{
    return kbo_get_current_yyyymmdd(out_date);
}

static uint32_t kbo_add_years_yyyymmdd(uint32_t yyyymmdd, uint32_t add_years)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1800u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0u;
    }
    year += add_years;
    if (year > 2200u) {
        year = 2200u;
    }
    int dim = kbo_days_in_month(year, month);
    if (dim <= 0) {
        return 0u;
    }
    if (day > (uint32_t)dim) {
        day = (uint32_t)dim;
    }
    return year * 10000u + month * 100u + day;
}


