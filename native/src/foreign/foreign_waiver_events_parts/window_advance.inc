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
    g_kbo_foreign_waiver_close_event_end_date = end_yyyymmdd;
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
    KBO_PROFILE_BEGIN(profile_foreign_waiver_advance);
    static uint32_t last_checked_yyyymmdd = 0u;
    static uint32_t last_checked_result = 0u;
    if (today_yyyymmdd != 0u
            && today_yyyymmdd == last_checked_yyyymmdd
            && last_checked_result != 0u) {
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.cached");
        return 0;
    }
    last_checked_yyyymmdd = today_yyyymmdd;
    last_checked_result = 0u;

    static uint32_t last_logged_evaluate_today = 0u;
    if (last_logged_evaluate_today != today_yyyymmdd) {
        last_logged_evaluate_today = today_yyyymmdd;
        append_logf(
            "foreign waiver auto: evaluate season-end check today=%u serial=%u prev_marker=%u",
            today_yyyymmdd, today_serial,
            g_kbo_foreign_waiver_last_seen_yyyymmdd);
    }

    if (today_yyyymmdd == 0 || today_serial == 0) {
        append_log_line("foreign waiver auto: season-end check skipped - invalid date");
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.invalid_date");
        return 0;
    }

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        append_log_line("foreign waiver auto: missing configured league id; skip season-end evaluation");
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.no_league_id");
        return 0;
    }

    uint32_t season_end_yyyymmdd = kbo_detect_offseason_starts_event(today_yyyymmdd, configured_league_id);
    if (season_end_yyyymmdd == 0u) {
        static uint32_t last_logged_no_marker_today = 0u;
        if (last_logged_no_marker_today != today_yyyymmdd) {
            last_logged_no_marker_today = today_yyyymmdd;
            append_logf("foreign waiver auto: no Offseason starts event found today=%u", today_yyyymmdd);
        }
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.no_offseason_marker");
        return 0;
    }
    if (today_yyyymmdd < season_end_yyyymmdd) {
        append_logf(
            "foreign waiver auto: waiting for Offseason starts marker season_end=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.waiting_marker");
        return 0;
    }

    uint32_t season_end_serial = kbo_date_serial(
        season_end_yyyymmdd / 10000u,
        (season_end_yyyymmdd / 100u) % 100u,
        season_end_yyyymmdd % 100u);
    if (season_end_serial == 0u) {
        append_logf(
            "foreign waiver auto: invalid Offseason starts serial season_end=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.invalid_marker_serial");
        return 0;
    }
    if (today_serial >= season_end_serial + 20u) {
        static uint32_t last_stale_marker = 0u;
        g_kbo_foreign_waiver_last_seen_yyyymmdd = season_end_yyyymmdd;
        if (last_stale_marker != season_end_yyyymmdd) {
            last_stale_marker = season_end_yyyymmdd;
            kbo_write_foreign_waiver_window(season_end_yyyymmdd, season_end_yyyymmdd, "stale_offseason_marker");
            g_kbo_foreign_waiver_start_event_date = season_end_yyyymmdd;
            g_kbo_foreign_waiver_close_event_end_date = season_end_yyyymmdd;
            append_logf(
                "foreign waiver auto: skipped stale season-end marker season_end=%u today=%u reason=window_already_elapsed",
                season_end_yyyymmdd,
                today_yyyymmdd);
        }
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.stale_marker");
        return 0;
    }
    if (g_kbo_foreign_waiver_last_seen_yyyymmdd == season_end_yyyymmdd) {
        static uint32_t last_logged_already_marker = 0u;
        if (last_logged_already_marker != season_end_yyyymmdd) {
            last_logged_already_marker = season_end_yyyymmdd;
            append_logf(
                "foreign waiver auto: already opened for marker=%u today=%u",
                season_end_yyyymmdd,
                today_yyyymmdd);
        }
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.already_opened");
        return 0;
    }

    g_kbo_foreign_waiver_last_seen_yyyymmdd = season_end_yyyymmdd;
    append_logf(
        "foreign waiver auto: season-end marker confirmed season_end=%u today=%u",
        season_end_yyyymmdd,
        today_yyyymmdd);
    int opened = kbo_open_foreign_waiver_window(
        season_end_yyyymmdd,
        season_end_serial,
        "offseason_starts_event");
    last_checked_result = opened ? 1u : 2u;
    KBO_PROFILE_END(profile_foreign_waiver_advance, opened ? "foreign_waiver.advance.opened" : "foreign_waiver.advance.open_failed");
    return opened;
}


