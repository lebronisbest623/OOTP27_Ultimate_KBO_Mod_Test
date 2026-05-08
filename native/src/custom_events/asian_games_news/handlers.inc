static int kbo_handle_asian_games_selection_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_selection_fired_date == event_yyyymmdd) {
        return 0;
    }
    int selected = kbo_select_asian_games_roster(event_yyyymmdd, source);
    if (selected <= 0) {
        append_logf(
            "KBO Asian Games selection deferred source=%s date=%u selected=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            selected);
        return 0;
    }
    g_kbo_asian_games_last_selection_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        event_yyyymmdd,
        "[KBO] Asian Games Roster Announced",
        "The KBO has announced Korea's Asian Games roster.",
        source);
    append_logf(
        "KBO Asian Games selection reached source=%s date=%u selected=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        selected);
    return 1;
}

static int kbo_handle_asian_games_departure_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_departure_fired_date == event_yyyymmdd) {
        return 0;
    }
    uint32_t action_yyyymmdd = kbo_asian_games_effective_action_date(event_yyyymmdd);
    int departed = kbo_asian_games_depart_selected_players(action_yyyymmdd, source);
    if (departed <= 0) {
        uint32_t current_year = 0;
        uint32_t current_month = 0;
        uint32_t current_day = 0;
        if (kbo_current_date_is_valid(&current_year, &current_month, &current_day)
                && current_year > (event_yyyymmdd / 10000u)) {
            append_logf(
                "KBO Asian Games departure closed stale source=%s date=%u current=%04u-%02u-%02u departed=%d",
                source != NULL ? source : "",
                event_yyyymmdd,
                current_year,
                current_month,
                current_day,
                departed);
            return 1;
        }
        append_logf(
            "KBO Asian Games departure deferred source=%s date=%u departed=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            departed);
        return 0;
    }
    g_kbo_asian_games_last_departure_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        action_yyyymmdd,
        "[KBO] Asian Games Players Depart",
        "Selected players have departed their clubs for Asian Games duty.",
        source);
    append_logf(
        "KBO Asian Games departure reached source=%s event_date=%u action_date=%u departed=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        action_yyyymmdd,
        departed);
    return 1;
}

static int kbo_handle_asian_games_final_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_final_fired_date == event_yyyymmdd) {
        return 0;
    }
    uint32_t action_yyyymmdd = kbo_asian_games_effective_action_date(event_yyyymmdd);
    int returned = kbo_asian_games_finalize_selected_players(action_yyyymmdd, source);
    int already_finalized = returned <= 0 ? kbo_asian_games_roster_already_finalized(source) : 0;
    if (returned <= 0 && !already_finalized) {
        uint32_t current_year = 0;
        uint32_t current_month = 0;
        uint32_t current_day = 0;
        if (kbo_current_date_is_valid(&current_year, &current_month, &current_day)
                && current_year > (event_yyyymmdd / 10000u)) {
            append_logf(
                "KBO Asian Games final closed stale source=%s date=%u current=%04u-%02u-%02u returned=%d",
                source != NULL ? source : "",
                event_yyyymmdd,
                current_year,
                current_month,
                current_day,
                returned);
            return 1;
        }
        append_logf(
            "KBO Asian Games final deferred source=%s date=%u returned=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            returned);
        return 0;
    }
    g_kbo_asian_games_last_final_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        action_yyyymmdd,
        "[KBO] Korea Wins Asian Games Gold",
        "Korea has won the Asian Games baseball title.",
        source);
    append_logf(
        "KBO Asian Games final reached source=%s event_date=%u action_date=%u returned=%d already_finalized=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        action_yyyymmdd,
        returned,
        already_finalized);
    return 1;
}
