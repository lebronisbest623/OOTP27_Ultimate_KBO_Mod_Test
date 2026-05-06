/* Foreign priority and military custom-event scheduling. Included from native/src/custom_events.inc. */

static uint32_t kbo_recent_phase_transition_offseason_anchor(uint32_t league_id, uint32_t today_yyyymmdd)
{
    if (league_id == 0u
            || today_yyyymmdd == 0u
            || g_kbo_custom_event_last_offseason_transition_anchor == 0u
            || g_kbo_custom_event_last_offseason_transition_anchor != g_kbo_foreign_priority_last_scheduled_date
            || g_kbo_custom_event_last_phase_league_id != league_id
            || g_kbo_custom_event_last_seen_league_phase > 1u) {
        return 0u;
    }

    uint32_t anchor = g_kbo_custom_event_last_offseason_transition_anchor;
    if (anchor > today_yyyymmdd) {
        return 0u;
    }

    uint32_t anchor_serial = kbo_date_serial(anchor / 10000u, (anchor / 100u) % 100u, anchor % 100u);
    uint32_t today_serial = kbo_date_serial(today_yyyymmdd / 10000u, (today_yyyymmdd / 100u) % 100u, today_yyyymmdd % 100u);
    if (anchor_serial == 0u || today_serial == 0u || today_serial < anchor_serial) {
        return 0u;
    }
    if (today_serial - anchor_serial > 70u) {
        return 0u;
    }
    return anchor;
}

static int kbo_schedule_foreign_priority_custom_events_at_anchor(
    const char* source,
    uint32_t today,
    uint32_t league_id,
    uint32_t offseason_starts_yyyymmdd)
{
    if (league_id == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=league_id_unavailable today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        return -1;
    }
    if (today == 0u || offseason_starts_yyyymmdd == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=anchor_unavailable today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        return -1;
    }

    if (offseason_starts_yyyymmdd > today) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=offseason_starts_in_future season_end=%u today=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            today);
        return 0;
    }

    uint32_t anchor_date = offseason_starts_yyyymmdd;
    uint32_t open_date = anchor_date;
    uint32_t close_date = kbo_add_days_yyyymmdd(anchor_date, 20u);
    uint32_t military_selection_date = kbo_add_one_month_yyyymmdd(anchor_date);
    if (close_date == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=close_date_invalid season_end=%u anchor=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            anchor_date);
        return -1;
    }

    int open_exists = kbo_custom_event_exists_for_date(
        league_id,
        open_date,
        1);
    int close_exists = kbo_custom_event_exists_for_date(
        league_id,
        close_date,
        0);
    int military_exists = military_selection_date == 0u
        || kbo_custom_event_exists_by_title_for_date(
            league_id,
            military_selection_date,
            g_kbo_military_selection_event_title);
    if (g_kbo_foreign_priority_last_scheduled_date == offseason_starts_yyyymmdd
            && open_exists
            && close_exists
            && military_exists) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=already_scheduled_for_season_end today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        return 0;
    }

    int created_open = 0;
    if (!open_exists) {
        created_open = create_kbo_league_event(
            open_date / 10000u,
            (open_date / 100u) % 100u,
            open_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_foreign_priority_open_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_close = 0;
    if (!close_exists) {
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
            && !military_exists) {
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

    open_exists = open_exists || created_open || kbo_custom_event_exists_for_date(
        league_id,
        open_date,
        1);
    close_exists = close_exists || created_close || kbo_custom_event_exists_for_date(
        league_id,
        close_date,
        0);
    military_exists = military_selection_date == 0u
        || military_exists
        || created_military
        || kbo_custom_event_exists_by_title_for_date(
            league_id,
            military_selection_date,
            g_kbo_military_selection_event_title);

    append_logf(
        "KBO custom event schedule source=%s season_end=%u anchor=%u open=%u close=%u military=%u created_open=%d created_close=%d created_military=%d ready=%d",
        source != NULL ? source : "",
        offseason_starts_yyyymmdd,
        anchor_date,
        open_date,
        close_date,
        military_selection_date,
        created_open,
        created_close,
        created_military,
        open_exists && close_exists && military_exists);

    if (!(open_exists && close_exists && military_exists)) {
        return -1;
    }
    g_kbo_foreign_priority_last_scheduled_date = offseason_starts_yyyymmdd;
    return created_open || created_close || created_military;
}

static int kbo_schedule_foreign_priority_custom_events_for_anchor(
    const char* source,
    uint32_t offseason_starts_yyyymmdd)
{
    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || today == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    return kbo_schedule_foreign_priority_custom_events_at_anchor(
        source,
        today,
        league_id,
        offseason_starts_yyyymmdd);
}

static int kbo_schedule_foreign_priority_custom_events(const char* source)
{
    uint32_t today = 0u;
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (!kbo_get_current_yyyymmdd(&today) || today == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }

    uint32_t offseason_starts_yyyymmdd = kbo_get_latest_offseason_starts_event(today);
    if (offseason_starts_yyyymmdd == 0u) {
        offseason_starts_yyyymmdd = kbo_detect_offseason_anchor_by_league_year(league_id, today, source);
    }
    if (offseason_starts_yyyymmdd == 0u) {
        offseason_starts_yyyymmdd = kbo_recent_phase_transition_offseason_anchor(league_id, today);
        if (offseason_starts_yyyymmdd != 0u) {
            append_logf(
                "KBO custom event schedule fallback source=%s reason=recent_phase_transition_anchor today=%u season_end=%u",
                source != NULL ? source : "",
                today,
                offseason_starts_yyyymmdd);
        }
    }
    if (offseason_starts_yyyymmdd == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=no_offseason_starts_event today=%u",
            source != NULL ? source : "",
            today);
        return -1;
    }

    return kbo_schedule_foreign_priority_custom_events_at_anchor(
        source,
        today,
        league_id,
        offseason_starts_yyyymmdd);
}


