/* Foreign priority and military custom-event scheduling. Included from native/src/custom_events.inc. */

static int kbo_schedule_foreign_priority_custom_events(const char* source)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return 0;
    }

    uint32_t today = year * 10000u + month * 100u + day;
    uint32_t offseason_starts_yyyymmdd = kbo_get_latest_offseason_starts_event(today);
    if (offseason_starts_yyyymmdd == 0u) {
        offseason_starts_yyyymmdd = kbo_detect_offseason_anchor_by_league_year(league_id, today, source);
        if (offseason_starts_yyyymmdd == 0u) {
            append_logf(
                "KBO custom event schedule skipped source=%s reason=no_offseason_starts_event today=%u",
                source != NULL ? source : "",
                today);
            return 0;
        }
    }
    if (offseason_starts_yyyymmdd > today) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=offseason_starts_in_future season_end=%u today=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            today);
        return 0;
    }
    if (g_kbo_foreign_priority_last_scheduled_date == offseason_starts_yyyymmdd) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=already_scheduled_for_season_end today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        return 0;
    }

    uint32_t anchor_date = offseason_starts_yyyymmdd;
    uint32_t close_date = kbo_add_days_yyyymmdd(anchor_date, 20u);
    uint32_t military_selection_date = kbo_add_one_month_yyyymmdd(anchor_date);
    if (close_date == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=close_date_invalid season_end=%u anchor=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            anchor_date);
        return 0;
    }

    int created_close = 0;
    if (!kbo_custom_event_exists_for_date(
            league_id,
            close_date,
            0)) {
        created_close = create_kbo_league_event(
            close_date / 10000u,
            (close_date / 100u) % 100u,
            close_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_foreign_priority_close_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_military = 0;
    if (military_selection_date != 0u
            && g_kbo_military_selection_last_scheduled_date != offseason_starts_yyyymmdd) {
        created_military = create_kbo_league_event(
            military_selection_date / 10000u,
            (military_selection_date / 100u) % 100u,
            military_selection_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_military_selection_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
        if (created_military) {
            g_kbo_military_selection_last_scheduled_date = offseason_starts_yyyymmdd;
        }
    }

    append_logf(
        "KBO custom event schedule source=%s season_end=%u anchor=%u close=%u military=%u created_close=%d created_military=%d",
        source != NULL ? source : "",
        offseason_starts_yyyymmdd,
        anchor_date,
        close_date,
        military_selection_date,
        created_close,
        created_military);

    g_kbo_foreign_priority_last_scheduled_date = offseason_starts_yyyymmdd;
    return created_close || created_military;
}


