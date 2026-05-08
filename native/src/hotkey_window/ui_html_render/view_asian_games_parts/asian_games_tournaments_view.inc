static void kbo_webview_append_asian_games_tournaments_view(KboWindowTextBuffer* buffer)
{
    uint32_t current_year = 0u;
    uint32_t current_month = 0u;
    uint32_t current_day = 0u;
    if (!kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        kbo_current_year_relaxed(&current_year);
    }
    if (current_year < 2026u || current_year > 2200u) {
        current_year = 2026u;
    }

    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);

    KboAsianGamesScheduleSeed schedules[64];
    memset(schedules, 0, sizeof(schedules));
    int count = 0;
    for (uint32_t year = 2026u; year <= 2200u && count < (int)(sizeof(schedules) / sizeof(schedules[0])); year++) {
        KboAsianGamesScheduleSeed schedule;
        if (kbo_get_asian_games_schedule_for_year(year, &schedule)) {
            schedules[count++] = schedule;
        }
    }

    int official_count = 0;
    int provisional_count = 0;
    int projected_count = 0;
    for (int i = 0; i < count; i++) {
        if (ascii_equals_ignore_case(schedules[i].status, "official")
                || ascii_equals_ignore_case(schedules[i].status, "confirmed")) {
            official_count++;
        } else if (ascii_equals_ignore_case(schedules[i].status, "provisional")) {
            provisional_count++;
        } else if (ascii_equals_ignore_case(schedules[i].status, "projected")) {
            projected_count++;
        }
    }

    char summary_text[256] = {0};
    if (count > 0) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Tournaments - %d Listed - Official: %d / Provisional: %d / Projected: %d",
            count,
            official_count,
            provisional_count,
            projected_count);
    } else {
        snprintf(summary_text, sizeof(summary_text), "View: Tournaments - Asian Games");
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agTournamentTable'><thead><tr>"
        "<th class='roPo' data-sort-type='number'>Year</th><th class='roName' data-sort-type='text'>Host</th>"
        "<th class='roDate' data-sort-type='date'>Tournament</th><th class='roClub' data-sort-type='date'>Selection</th>"
        "<th class='roTeam' data-sort-type='date'>Departure</th><th class='roReturn' data-sort-type='date'>Final</th>"
        "<th class='roStatus' data-sort-type='text'>Status</th><th class='roResult' data-sort-type='text'>Phase</th>"
        "</tr></thead><tbody>");

    if (count <= 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8' class='roEmptyMessage'></td></tr>");
    } else {
        for (int i = 0; i < count; i++) {
            kbo_webview_append_asian_games_tournament_row(buffer, &schedules[i], today);
        }
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
