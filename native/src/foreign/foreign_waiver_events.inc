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

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign priority negotiation: failed to write window file error=%lu", GetLastError());
        return 0;
    }

    char line[128] = {0};
    int len = snprintf(line, sizeof(line), "%08u,%08u\r\n", start_yyyymmdd, end_yyyymmdd);
    DWORD written = 0;
    int ok = WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len;
    if (!ok || !kbo_atomic_commit(file, tmp_path, path)) {
        if (!ok) {
            kbo_atomic_commit(file, tmp_path, path);
            DeleteFileA(path);
        }
        ok = 0;
    }

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


