/* Read-only offseason phase transition scheduling. Included from native/src/custom_events.inc. */

static int kbo_custom_event_phase_is_offseason(uint8_t phase)
{
    return phase == 0u || phase == 1u;
}

static int kbo_custom_event_phase_can_enter_offseason(uint8_t phase)
{
    return phase == 3u || phase == 4u;
}

static int kbo_custom_event_date_allows_offseason_transition(uint32_t yyyymmdd)
{
    uint32_t month_day = yyyymmdd % 10000u;
    return month_day >= 1001u;
}

static int kbo_custom_event_read_league_phase(
    uint32_t league_id,
    uintptr_t* out_league_ptr,
    uint32_t* out_league_year,
    uint8_t* out_phase,
    uint32_t* out_phase_year)
{
    if (out_league_ptr != NULL) { *out_league_ptr = 0; }
    if (out_league_year != NULL) { *out_league_year = 0; }
    if (out_phase != NULL) { *out_phase = 0xffu; }
    if (out_phase_year != NULL) { *out_phase_year = 0; }
    if (league_id == 0u) {
        return 0;
    }

    uintptr_t league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
    if (league_ptr == 0
            || !memory_range_readable((void*)league_ptr, OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    uint32_t league_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    uint32_t phase_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
    if (phase > 4u || league_year < 1982u || league_year > 2200u) {
        return 0;
    }

    if (out_league_ptr != NULL) { *out_league_ptr = league_ptr; }
    if (out_league_year != NULL) { *out_league_year = league_year; }
    if (out_phase != NULL) { *out_phase = phase; }
    if (out_phase_year != NULL) { *out_phase_year = phase_year; }
    return 1;
}

static int kbo_custom_event_schedule_pending_offseason_transition(
    uint32_t today_yyyymmdd,
    const char* source)
{
    if (g_kbo_custom_event_pending_offseason_transition_anchor == 0u) {
        return 0;
    }
    if (g_kbo_custom_event_last_offseason_transition_anchor
            == g_kbo_custom_event_pending_offseason_transition_anchor) {
        g_kbo_custom_event_pending_offseason_transition_anchor = 0u;
        return 0;
    }
    if (today_yyyymmdd < g_kbo_custom_event_pending_offseason_transition_anchor) {
        return 0;
    }

    int scheduled = kbo_schedule_foreign_priority_custom_events_for_anchor(
        source,
        g_kbo_custom_event_pending_offseason_transition_anchor);
    append_logf(
        "KBO custom event offseason transition schedule source=%s today=%u anchor=%u result=%d",
        source != NULL ? source : "",
        today_yyyymmdd,
        g_kbo_custom_event_pending_offseason_transition_anchor,
        scheduled);
    if (scheduled >= 0) {
        g_kbo_custom_event_last_offseason_transition_anchor =
            g_kbo_custom_event_pending_offseason_transition_anchor;
        g_kbo_custom_event_pending_offseason_transition_anchor = 0u;
        return 1;
    }
    return 0;
}

static int kbo_custom_event_monitor_check_offseason_transition(
    uint32_t today_yyyymmdd,
    const char* source)
{
    if (today_yyyymmdd == 0u) {
        return 0;
    }
    if (!kbo_custom_event_date_allows_offseason_transition(today_yyyymmdd)) {
        return kbo_custom_event_schedule_pending_offseason_transition(today_yyyymmdd, source);
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }

    uintptr_t league_ptr = 0;
    uint32_t league_year = 0;
    uint32_t phase_year = 0;
    uint8_t phase = 0xffu;
    if (!kbo_custom_event_read_league_phase(
            league_id,
            &league_ptr,
            &league_year,
            &phase,
            &phase_year)) {
        return kbo_custom_event_schedule_pending_offseason_transition(today_yyyymmdd, source);
    }

    int same_league = g_kbo_custom_event_last_phase_league_id == league_id
        && g_kbo_custom_event_last_phase_league_ptr == league_ptr;
    int had_previous = same_league && g_kbo_custom_event_last_seen_league_phase <= 4u;
    int transitioned_to_offseason = had_previous
        && kbo_custom_event_phase_can_enter_offseason(g_kbo_custom_event_last_seen_league_phase)
        && kbo_custom_event_phase_is_offseason(phase);

    if (!same_league
            || g_kbo_custom_event_last_phase_date != today_yyyymmdd
            || g_kbo_custom_event_last_phase_league_year != league_year
            || g_kbo_custom_event_last_seen_league_phase != phase
            || g_kbo_custom_event_last_phase_year != phase_year) {
        append_logf(
            "KBO custom event phase observed source=%s league=%p league_id=%u date=%u league_year=%u previous_phase=%u phase=%u phase_year=%u",
            source != NULL ? source : "",
            (void*)league_ptr,
            league_id,
            today_yyyymmdd,
            league_year,
            had_previous ? (unsigned)g_kbo_custom_event_last_seen_league_phase : 255u,
            (unsigned)phase,
            phase_year);
    }

    if (transitioned_to_offseason) {
        if (kbo_custom_event_date_allows_offseason_transition(today_yyyymmdd)) {
            if (g_kbo_custom_event_last_offseason_transition_anchor != today_yyyymmdd) {
                g_kbo_custom_event_pending_offseason_transition_anchor = today_yyyymmdd;
                append_logf(
                    "KBO custom event offseason transition detected source=%s league=%p league_id=%u date=%u league_year=%u previous_phase=%u phase=%u",
                    source != NULL ? source : "",
                    (void*)league_ptr,
                    league_id,
                    today_yyyymmdd,
                    league_year,
                    (unsigned)g_kbo_custom_event_last_seen_league_phase,
                    (unsigned)phase);
            }
        } else {
            append_logf(
                "KBO custom event offseason transition ignored source=%s reason=date_window date=%u previous_phase=%u phase=%u",
                source != NULL ? source : "",
                today_yyyymmdd,
                (unsigned)g_kbo_custom_event_last_seen_league_phase,
                (unsigned)phase);
        }
    }

    g_kbo_custom_event_last_phase_league_id = league_id;
    g_kbo_custom_event_last_phase_league_ptr = league_ptr;
    g_kbo_custom_event_last_phase_date = today_yyyymmdd;
    g_kbo_custom_event_last_phase_league_year = league_year;
    g_kbo_custom_event_last_phase_year = phase_year;
    g_kbo_custom_event_last_seen_league_phase = phase;

    return kbo_custom_event_schedule_pending_offseason_transition(today_yyyymmdd, source);
}
