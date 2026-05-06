/* Asian Games custom-event scheduling. Included from native/src/custom_events.inc. */

static int kbo_schedule_asian_games_custom_events(const char* source)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        append_logf(
            "KBO Asian Games schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }
    if (!kbo_year_has_asian_games(year)) {
        return 0;
    }
    if (g_kbo_asian_games_last_scheduled_year == year) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        append_logf(
            "KBO Asian Games schedule skipped source=%s reason=league_id_unavailable year=%u",
            source != NULL ? source : "",
            year);
        return -1;
    }

    uint32_t selection_date = year * 10000u + 805u;
    uint32_t departure_date = year * 10000u + 919u;
    uint32_t final_date = year * 10000u + 1004u;
    uint32_t today = year * 10000u + month * 100u + day;
    if (today > final_date) {
        g_kbo_asian_games_last_scheduled_year = year;
        append_logf(
            "KBO Asian Games schedule skipped source=%s reason=events_past year=%u today=%u final=%u",
            source != NULL ? source : "",
            year,
            today,
            final_date);
        return 0;
    }

    int created_selection = 0;
    int created_departure = 0;
    int created_final = 0;
    int skipped_past_selection = selection_date < today;
    int skipped_past_departure = departure_date < today;
    int skipped_past_final = final_date < today;
    int selection_exists = skipped_past_selection || kbo_custom_event_exists_by_title_for_date(
            league_id,
            selection_date,
            g_kbo_asian_games_selection_event_title);
    int departure_exists = skipped_past_departure || kbo_custom_event_exists_by_title_for_date(
            league_id,
            departure_date,
            g_kbo_asian_games_departure_event_title);
    int final_exists = skipped_past_final || kbo_custom_event_exists_by_title_for_date(
            league_id,
            final_date,
            g_kbo_asian_games_final_event_title);

    if (!selection_exists) {
        created_selection = create_kbo_league_event(
            year,
            8u,
            5u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_asian_games_selection_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!departure_exists) {
        created_departure = create_kbo_league_event(
            year,
            9u,
            19u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_asian_games_departure_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!final_exists) {
        created_final = create_kbo_league_event(
            year,
            10u,
            4u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_asian_games_final_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    selection_exists = skipped_past_selection || created_selection || kbo_custom_event_exists_by_title_for_date(
        league_id,
        selection_date,
        g_kbo_asian_games_selection_event_title);
    departure_exists = skipped_past_departure || created_departure || kbo_custom_event_exists_by_title_for_date(
        league_id,
        departure_date,
        g_kbo_asian_games_departure_event_title);
    final_exists = skipped_past_final || created_final || kbo_custom_event_exists_by_title_for_date(
        league_id,
        final_date,
        g_kbo_asian_games_final_event_title);
    if (selection_exists && departure_exists && final_exists) {
        g_kbo_asian_games_last_scheduled_year = year;
    }
    append_logf(
        "KBO Asian Games schedule source=%s year=%u today=%u selection=%u departure=%u final=%u created_selection=%d created_departure=%d created_final=%d skipped_past_selection=%d skipped_past_departure=%d skipped_past_final=%d ready=%d",
        source != NULL ? source : "",
        year,
        today,
        selection_date,
        departure_date,
        final_date,
        created_selection,
        created_departure,
        created_final,
        skipped_past_selection,
        skipped_past_departure,
        skipped_past_final,
        selection_exists && departure_exists && final_exists);
    if (!(selection_exists && departure_exists && final_exists)) {
        return -1;
    }
    return created_selection || created_departure || created_final;
}


