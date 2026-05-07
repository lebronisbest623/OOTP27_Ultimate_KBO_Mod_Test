static int kbo_webview_asian_games_schedule(KboAsianGamesScheduleSeed* out)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        kbo_current_year_relaxed(&year);
    }
    if (year < 1982u || year > 2200u) {
        return 0;
    }
    return kbo_get_next_asian_games_schedule(year, out);
}

static void kbo_webview_format_asian_games_date(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static void kbo_webview_format_asian_games_date_range(uint32_t start, uint32_t end, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (start == 0u && end == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    if (start == 0u || end == 0u || start == end) {
        kbo_webview_format_asian_games_date(start != 0u ? start : end, out, out_size);
        return;
    }

    char start_text[16] = {0};
    char end_text[16] = {0};
    kbo_webview_format_asian_games_date(start, start_text, sizeof(start_text));
    kbo_webview_format_asian_games_date(end, end_text, sizeof(end_text));
    snprintf(out, out_size, "%s - %s", start_text, end_text);
}

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

static int kbo_webview_weekday_for_yyyymmdd(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    st.wYear = (WORD)year;
    st.wMonth = (WORD)month;
    st.wDay = (WORD)day;
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        return -1;
    }
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    uint64_t days_since_1601 = value.QuadPart / 10000000ull / 86400ull;
    return (int)((days_since_1601 + 1ull) % 7ull);
}

static const char* kbo_webview_asian_games_schedule_status(
    uint32_t event_date,
    const char* event_title,
    uint32_t fired_date,
    uint32_t today,
    int event_exists,
    int auto_schedule,
    const char** out_class)
{
    if (event_date == 0u) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "TBD";
    }
    int processed = (fired_date == event_date)
        || kbo_custom_event_processed_marker_exists(event_date, event_title);
    if (processed) {
        if (out_class != NULL) { *out_class = "roReady"; }
        return "Complete";
    }
    if (today == event_date) {
        if (out_class != NULL) { *out_class = "roOrange"; }
        return "Today";
    }
    if (today != 0u && today > event_date) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "Past";
    }
    if (event_exists) {
        if (out_class != NULL) { *out_class = "roServing"; }
        return "Scheduled";
    }
    if (!auto_schedule) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "Seeded";
    }
    if (out_class != NULL) { *out_class = "roSoon"; }
    return "Pending";
}

static void kbo_webview_append_asian_games_schedule_row(
    KboWindowTextBuffer* buffer,
    uint32_t event_date,
    const char* event_title,
    const char* event_label,
    const char* action_text,
    const char* impact_text,
    uint32_t fired_date,
    uint32_t today,
    uint32_t league_id,
    int auto_schedule)
{
    int event_exists = league_id != 0u && event_date != 0u
        ? kbo_custom_event_exists_by_title_for_date(league_id, event_date, event_title)
        : 0;
    const char* status_class = "";
    const char* status = kbo_webview_asian_games_schedule_status(
        event_date,
        event_title,
        fired_date,
        today,
        event_exists,
        auto_schedule,
        &status_class);

    char date_text[16] = {0};
    const char* weekday = "";
    if (event_date != 0u) {
        snprintf(
            date_text,
            sizeof(date_text),
            "%04u-%02u-%02u",
            event_date / 10000u,
            (event_date / 100u) % 100u,
            event_date % 100u);
        weekday = kbo_hub_weekday_abbrev(kbo_webview_weekday_for_yyyymmdd(event_date));
    } else {
        snprintf(date_text, sizeof(date_text), "TBD");
    }

    kbo_window_text_appendf(
        buffer,
        "<tr><td class='roDate'>%s</td><td class='roPo'>%s</td><td class='roName'>",
        date_text,
        weekday);
    kbo_html_append_escaped(buffer, event_label);
    kbo_window_text_appendf(buffer, "</td><td class='roLeague'>");
    kbo_html_append_escaped(buffer, action_text);
    kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
    kbo_html_append_escaped(buffer, impact_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus %s'>", status_class);
    kbo_html_append_escaped(buffer, status);
    kbo_window_text_appendf(buffer, "</td></tr>");
}

static void kbo_webview_append_asian_games_schedule_view(KboWindowTextBuffer* buffer)
{
    KboAsianGamesScheduleSeed schedule;
    int has_schedule = kbo_webview_asian_games_schedule(&schedule);
    uint32_t schedule_year = has_schedule ? schedule.year : 0u;
    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }

    uint32_t selection_date = has_schedule ? schedule.selection_date : 0u;
    uint32_t departure_date = has_schedule ? schedule.departure_date : 0u;
    uint32_t final_date = has_schedule ? schedule.final_date : 0u;
    int auto_schedule = has_schedule ? kbo_asian_games_schedule_auto_events_enabled(&schedule) : 0;

    int completed = 0;
    if (selection_date != 0u && kbo_custom_event_processed_marker_exists(selection_date, g_kbo_asian_games_selection_event_title)) { completed++; }
    if (departure_date != 0u && kbo_custom_event_processed_marker_exists(departure_date, g_kbo_asian_games_departure_event_title)) { completed++; }
    if (final_date != 0u && kbo_custom_event_processed_marker_exists(final_date, g_kbo_asian_games_final_event_title)) { completed++; }

    char summary_text[256] = {0};
    if (schedule_year != 0u && kbo_asian_games_schedule_has_event_dates(&schedule)) {
        char host_text[128] = {0};
        if (schedule.host_city[0] != '\0' && schedule.host_country[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s, %s", schedule.host_city, schedule.host_country);
        } else if (schedule.host_city[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s", schedule.host_city);
        }
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Schedule - Asian Games %u%s - %s - %d Completed / 3 Events",
            schedule_year,
            host_text,
            kbo_asian_games_schedule_status_label(&schedule),
            completed);
    } else if (schedule_year != 0u) {
        char host_text[128] = {0};
        if (schedule.host_city[0] != '\0' && schedule.host_country[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s, %s", schedule.host_city, schedule.host_country);
        } else if (schedule.host_city[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s", schedule.host_city);
        }
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Schedule - Asian Games %u%s - %s / Dates TBD",
            schedule_year,
            host_text,
            kbo_asian_games_schedule_status_label(&schedule));
    } else {
        snprintf(summary_text, sizeof(summary_text), "View: Schedule - Asian Games");
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agScheduleTable'><thead><tr>"
        "<th class='roDate' data-sort-type='date'>Date</th><th class='roPo' data-sort-type='text'>Day</th>"
        "<th class='roName' data-sort-type='text'>Event</th><th class='roLeague' data-sort-type='text'>Action</th>"
        "<th class='roClub' data-sort-type='text'>Impact</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    if (schedule_year == 0u) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6' class='roEmptyMessage'></td></tr>");
    } else {
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            selection_date,
            g_kbo_asian_games_selection_event_title,
            kbo_hub_text("\xeb\x8c\x80\xed\x91\x9c\xed\x8c\x80 \xeb\xb0\x9c\xed\x91\x9c", "Roster Selection"),
            kbo_hub_text("KBO \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0 \xec\x84\xa0\xeb\xb0\x9c", "KBO announces the national-team roster"),
            kbo_hub_text("24\xeb\xaa\x85 \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "24-man roster"),
            g_kbo_asian_games_last_selection_fired_date,
            today,
            league_id,
            auto_schedule);
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            departure_date,
            g_kbo_asian_games_departure_event_title,
            kbo_hub_text("\xec\x84\xa0\xec\x88\x98\xeb\x8b\xa8 \xec\xb6\x9c\xea\xb5\xad", "Player Departure"),
            kbo_hub_text("\xec\x84\xa0\xeb\xb0\x9c \xec\x84\xa0\xec\x88\x98 \xea\xb5\xac\xeb\x8b\xa8 \xec\x9d\xb4\xed\x83\x88 \xeb\xb0\x8f \xeb\x8c\x80\xec\xb2\xb4 \xed\x99\x95\xec\x9d\xb8", "Selected players leave their clubs"),
            kbo_hub_text("\xec\xa0\x9c\xed\x95\x9c \xeb\xaa\x85\xeb\x8b\xa8", "Restricted-list window"),
            g_kbo_asian_games_last_departure_fired_date,
            today,
            league_id,
            auto_schedule);
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            final_date,
            g_kbo_asian_games_final_event_title,
            kbo_hub_text("\xea\xb2\xb0\xec\x8a\xb9 / \xeb\xb3\xb5\xea\xb7\x80", "Final / Return"),
            kbo_hub_text("\xeb\x8c\x80\xed\x9a\x8c \xec\xa2\x85\xeb\xa3\x8c \xed\x9b\x84 \xea\xb5\xac\xeb\x8b\xa8 \xeb\xb3\xb5\xea\xb7\x80 \xeb\xb0\x8f \xeb\xb3\x91\xec\x97\xad \xed\x98\x9c\xed\x83\x9d \xec\xb2\x98\xeb\xa6\xac", "Players return after the final"),
            kbo_hub_text("\xea\xb8\x88\xeb\xa9\x94\xeb\x8b\xac \xeb\xa9\xb4\xec\xa0\x9c", "Gold-medal exemption"),
            g_kbo_asian_games_last_final_fired_date,
            today,
            league_id,
            auto_schedule);
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}

static void kbo_webview_append_asian_games_view(KboWindowTextBuffer* buffer)
{
    if (g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS) {
        kbo_webview_append_asian_games_tournaments_view(buffer);
        return;
    }
    if (g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_SCHEDULE) {
        kbo_webview_append_asian_games_schedule_view(buffer);
        return;
    }

            kbo_clear_asian_games_roster_if_save_changed("hotkey_ui");
            LONG roster_count = g_kbo_asian_games_roster_count;
            if (roster_count <= 0) {
                kbo_load_asian_games_roster_csv("hotkey_ui");
                roster_count = g_kbo_asian_games_roster_count;
            }
            if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
                roster_count = 0;
            }
            kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
            kbo_webview_append_roster_top_bar(buffer, NULL);
            kbo_window_text_appendf(
                buffer,
                "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agRosterTable'><thead><tr>"
                "<th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th><th class='roName' data-sort-type='text'>Name</th>"
                "<th class='roLeague' data-sort-type='text'>League</th><th class='roAge' data-sort-type='number'>Age</th><th class='roNat' data-sort-type='text'>Nationality*</th>"
                "<th class='roTeam' data-sort-type='text'>Team</th><th class='roClub' data-sort-type='text'>WC</th></tr></thead><tbody>");
            if (roster_count == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='8'></td></tr>");
            } else {
                for (LONG i = 0; i < roster_count; i++) {
                    KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
                    uintptr_t player_ptr = entry->player_ptr;
                    if (!kbo_player_pointer_plausible(player_ptr)) {
                        player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
                    }
                    char player_name[96] = {0};
                    if (kbo_player_pointer_plausible(player_ptr)) {
                        kbo_hub_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
                    } else {
                        snprintf(player_name, sizeof(player_name), "Unknown player");
                    }
                    char uniform_number[8] = {0};
                    kbo_webview_copy_player_uniform_number(entry->player_id, uniform_number, sizeof(uniform_number));
                    char team_abbrev[16] = {0};
                    kbo_hub_copy_team_abbrev_by_id(entry->original_team_id, team_abbrev, sizeof(team_abbrev), NULL);
                    kbo_window_text_appendf(
                        buffer,
                        "<tr><td class='roPo'>%s</td><td class='roNum'>",
                        kbo_webview_player_position_label(kbo_player_pointer_plausible(player_ptr) ? (uint8_t*)player_ptr : NULL, entry->role));
                    kbo_html_append_escaped(buffer, uniform_number);
                    kbo_window_text_appendf(buffer, "</td><td class='roName'>");
                    kbo_html_append_escaped(buffer, player_name);
                    kbo_window_text_appendf(
                        buffer,
                        "</td><td class='roLeague'>Korean National Team</td><td class='roAge'>%u</td>",
                        (uint32_t)entry->age
                    );
                    kbo_webview_append_roster_nation_cell(buffer, OOTP27_KBO_KOREA_NATION_ID);
                    kbo_window_text_appendf(buffer, "<td class='roTeam'>");
                    kbo_html_append_escaped(buffer, team_abbrev);
                    kbo_window_text_appendf(
                        buffer,
                        "</td><td class='roClub'>%s</td></tr>",
                        entry->wildcard ? "YES" : "NO");
                }
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
