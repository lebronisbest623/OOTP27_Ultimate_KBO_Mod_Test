static int kbo_military_resolve_application_window(
    uint32_t* out_today,
    uint32_t* out_anchor,
    uint32_t* out_announcement)
{
    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || today == 0u) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }

    uint32_t anchor = kbo_get_latest_offseason_starts_event(today);
    if (anchor == 0u) {
        anchor = kbo_recent_phase_transition_offseason_anchor(league_id, today);
    }
    if (anchor == 0u
            && g_kbo_custom_event_last_offseason_transition_anchor != 0u
            && g_kbo_custom_event_last_offseason_transition_anchor <= today) {
        anchor = g_kbo_custom_event_last_offseason_transition_anchor;
    }
    if (anchor == 0u
            && g_kbo_foreign_priority_last_scheduled_date != 0u
            && g_kbo_foreign_priority_last_scheduled_date <= today) {
        anchor = g_kbo_foreign_priority_last_scheduled_date;
    }

    uint32_t announcement = kbo_add_one_month_yyyymmdd(anchor);
    if (out_today != NULL) { *out_today = today; }
    if (out_anchor != NULL) { *out_anchor = anchor; }
    if (out_announcement != NULL) { *out_announcement = announcement; }
    if (anchor == 0u || announcement == 0u) {
        return 0;
    }
    return today >= anchor && today <= announcement;
}

static int kbo_military_applicant_position_bucket(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))) {
        return 4;
    }
    switch (player[OOTP27_PLAYER_POSITION_GROUP_OFFSET]) {
    case 1u: return 0;
    case 2u: return 1;
    case 3u:
    case 4u:
    case 5u:
    case 6u: return 2;
    case 7u:
    case 8u:
    case 9u: return 3;
    default: break;
    }
    return 4;
}

static void kbo_military_refresh_applicants_for_hotkey_view(void)
{
    uint32_t entry_year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&entry_year, &month, &day)) {
        kbo_current_year_relaxed(&entry_year);
    }
    if (entry_year < 1982u || entry_year > 2300u) {
        return;
    }

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint32_t sang_id = sang != NULL && memory_range_readable(sang, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET)
        : 0u;
    if (sang_id == 0u) {
        return;
    }
    kbo_refresh_military_selection_candidates_from_memory(
        (uint16_t)entry_year,
        sang_id,
        "hotkey_ui_applicants");
}
