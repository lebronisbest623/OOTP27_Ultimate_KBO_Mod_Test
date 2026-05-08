static int kbo_webview_weekday_for_yyyymmdd(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    st.wYear = (WORD)year;
    st.wMonth = (WORD)month;
    st.wDay = (WORD)day;
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        return -1;
    }
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    uint64_t days_since_1601 = value.QuadPart / 10000000ull / 86400ull;
    return (int)((days_since_1601 + 1ull) % 7ull);
}

static const char* kbo_webview_asian_games_schedule_status(
    uint32_t event_date,
    const char* event_title,
    uint32_t fired_date,
    uint32_t today,
    int event_exists,
    int auto_schedule,
    const char** out_class)
{
    if (event_date == 0u) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "TBD";
    }
    int processed = (fired_date == event_date)
        || kbo_custom_event_processed_marker_exists(event_date, event_title);
    if (processed) {
        if (out_class != NULL) { *out_class = "roReady"; }
        return "Complete";
    }
    if (today == event_date) {
        if (out_class != NULL) { *out_class = "roOrange"; }
        return "Today";
    }
    if (today != 0u && today > event_date) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "Past";
    }
    if (event_exists) {
        if (out_class != NULL) { *out_class = "roServing"; }
        return "Scheduled";
    }
    if (!auto_schedule) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "Seeded";
    }
    if (out_class != NULL) { *out_class = "roSoon"; }
    return "Pending";
}
