/* Foreign waiver negotiation-window file reads and open-state checks. Included from native/src/foreign_waiver_ai.inc. */

static int kbo_read_foreign_waiver_window(uint32_t* out_start, uint32_t* out_end)
{
    if (out_start == NULL || out_end == NULL) {
        return 0;
    }
    *out_start = 0;
    *out_end = 0;

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_event_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char raw[128] = {0};
    DWORD read = 0;
    if (!ReadFile(file, raw, sizeof(raw) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    if (read == 0) {
        return 0;
    }

    char* first = raw;
    char* second = NULL;
    while (*first == ' ' || *first == '\r' || *first == '\n' || *first == '\t') {
        first++;
    }
    if (*first == '#') {
        return 0;
    }
    char* sep = strchr(first, ',');
    if (sep == NULL) {
        return 0;
    }
    *sep = '\0';
    second = sep + 1;
    while (*second == ' ' || *second == '\r' || *second == '\n' || *second == '\t') {
        second++;
    }

    if (!kbo_parse_yyyymmdd(first, out_start)) {
        return 0;
    }
    if (!kbo_parse_yyyymmdd(second, out_end)) {
        return 0;
    }
    return 1;
}

static int kbo_read_foreign_waiver_window_cached(uint32_t today, uint32_t* out_start, uint32_t* out_end)
{
    static uint32_t cached_today = 0u;
    static uint32_t cached_start = 0u;
    static uint32_t cached_end = 0u;
    static int cached_ok = 0;
    static DWORD cached_tick = 0u;

    if (out_start == NULL || out_end == NULL) {
        return 0;
    }
    *out_start = 0u;
    *out_end = 0u;

    DWORD now = GetTickCount();
    if (cached_today != today || cached_tick == 0u || now - cached_tick > 2000u) {
        uint32_t start = 0u;
        uint32_t end = 0u;
        cached_ok = kbo_read_foreign_waiver_window(&start, &end);
        cached_start = start;
        cached_end = end;
        cached_today = today;
        cached_tick = now;
    }

    if (!cached_ok) {
        return 0;
    }
    *out_start = cached_start;
    *out_end = cached_end;
    return 1;
}

static int kbo_is_foreign_waiver_negotiation_window_open(void)
{
    if (!kbo_fix_enabled() || !kbo_foreign_waiver_ai_enabled()) {
        return 0;
    }

    uint32_t today = 0;
    if (!kbo_get_current_yyyymmdd(&today)) {
        return 0;
    }

    uint32_t today_serial = kbo_date_serial(today / 10000u, (today / 100u) % 100u, today % 100u);
    if (kbo_foreign_waiver_legacy_auto_detector_enabled()) {
        kbo_advance_foreign_waiver_window(today, today_serial);
    } else {
        static LONG logged_legacy_disabled = 0;
        if (InterlockedCompareExchange(&logged_legacy_disabled, 1, 0) == 0) {
            append_log_line("foreign waiver legacy auto detector disabled: kbo_flags.json disable_foreign_waiver_legacy_auto_detector is true");
        }
    }

    uint32_t start = 0;
    uint32_t end = 0;
    if (!kbo_read_foreign_waiver_window_cached(today, &start, &end)) {
        int open = (g_kbo_foreign_waiver_window_end_serial != 0
                && today_serial >= g_kbo_foreign_waiver_window_start_serial
                && today_serial < g_kbo_foreign_waiver_window_end_serial);
        if (!open && g_kbo_foreign_waiver_window_end_serial != 0
                && today_serial > g_kbo_foreign_waiver_window_end_serial) {
            if (g_kbo_foreign_waiver_last_close_event_end != g_kbo_foreign_waiver_window_end_serial) {
                g_kbo_foreign_waiver_last_close_event_end = g_kbo_foreign_waiver_window_end_serial;
                append_logf(
                    "foreign priority negotiation: event closed today=%u end_serial=%u source=auto detector",
                    today,
                    g_kbo_foreign_waiver_window_end_serial);
            }
        }
        return open;
    }

    if (today >= start && today < end) {
        return 1;
    }
    return 0;
}


