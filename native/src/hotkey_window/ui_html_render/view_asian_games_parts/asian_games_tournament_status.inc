static const char* kbo_webview_asian_games_tournament_status_class(const KboAsianGamesScheduleSeed* schedule)
{
    if (schedule == NULL || schedule->status[0] == '\0') {
        return "roServing";
    }
    if (ascii_equals_ignore_case(schedule->status, "official")
            || ascii_equals_ignore_case(schedule->status, "confirmed")) {
        return "roReady";
    }
    if (ascii_equals_ignore_case(schedule->status, "provisional")) {
        return "roSoon";
    }
    if (ascii_equals_ignore_case(schedule->status, "projected")) {
        return "roMuted";
    }
    return "roServing";
}

static const char* kbo_webview_asian_games_tournament_phase(
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today,
    const char** out_class)
{
    if (out_class != NULL) {
        *out_class = "roServing";
    }
    if (schedule == NULL || schedule->year == 0u || today == 0u) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "TBD";
    }
    if (schedule->tournament_start != 0u
            && schedule->tournament_end != 0u
            && today >= schedule->tournament_start
            && today <= schedule->tournament_end) {
        if (out_class != NULL) { *out_class = "roOrange"; }
        return "In Progress";
    }
    if (schedule->final_date != 0u && today > schedule->final_date) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "Completed";
    }
    if (schedule->selection_date != 0u
            && schedule->tournament_start != 0u
            && today >= schedule->selection_date
            && today < schedule->tournament_start) {
        if (out_class != NULL) { *out_class = "roSoon"; }
        return "Roster Window";
    }
    return "Upcoming";
}

static void kbo_webview_append_asian_games_tournament_row(
    KboWindowTextBuffer* buffer,
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today)
{
    if (buffer == NULL || schedule == NULL || schedule->year == 0u) {
        return;
    }

    char host_text[128] = {0};
    if (schedule->host_city[0] != '\0' && schedule->host_country[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s, %s", schedule->host_city, schedule->host_country);
    } else if (schedule->host_city[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s", schedule->host_city);
    } else if (schedule->host_country[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s", schedule->host_country);
    } else {
        snprintf(host_text, sizeof(host_text), "TBD");
    }

    char tournament_text[40] = {0};
    char selection_text[16] = {0};
    char departure_text[16] = {0};
    char final_text[16] = {0};
    kbo_webview_format_asian_games_date_range(
        schedule->tournament_start,
        schedule->tournament_end,
        tournament_text,
        sizeof(tournament_text));
    kbo_webview_format_asian_games_date(schedule->selection_date, selection_text, sizeof(selection_text));
    kbo_webview_format_asian_games_date(schedule->departure_date, departure_text, sizeof(departure_text));
    kbo_webview_format_asian_games_date(schedule->final_date, final_text, sizeof(final_text));

    const char* phase_class = "";
    const char* phase = kbo_webview_asian_games_tournament_phase(schedule, today, &phase_class);
    const char* status_class = kbo_webview_asian_games_tournament_status_class(schedule);
    const char* status = kbo_asian_games_schedule_status_label(schedule);

    kbo_window_text_appendf(
        buffer,
        "<tr><td class='roPo'>%u</td><td class='roName'>",
        schedule->year);
    kbo_html_append_escaped(buffer, host_text);
    kbo_window_text_appendf(buffer, "</td><td class='roDate'>");
    kbo_html_append_escaped(buffer, tournament_text);
    kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
    kbo_html_append_escaped(buffer, selection_text);
    kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
    kbo_html_append_escaped(buffer, departure_text);
    kbo_window_text_appendf(buffer, "</td><td class='roReturn'>");
    kbo_html_append_escaped(buffer, final_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus %s'>", status_class);
    kbo_html_append_escaped(buffer, status);
    kbo_window_text_appendf(buffer, "</td><td class='roResult %s'>", phase_class);
    kbo_html_append_escaped(buffer, phase);
    kbo_window_text_appendf(buffer, "</td></tr>");
}
