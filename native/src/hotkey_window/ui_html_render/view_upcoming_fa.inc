static void kbo_webview_append_upcoming_fa_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    uint32_t current_year = 0u;
    kbo_current_year_relaxed(&current_year);

    char team_name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(g_kbo_hub_selected_team_id, team_name, sizeof(team_name), NULL);

    char summary_text[256] = {0};
    if (current_year != 0u) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Upcoming FA - %s - Season %u",
            team_name[0] != '\0' ? team_name : "Selected club",
            current_year);
    } else {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Upcoming FA - %s",
            team_name[0] != '\0' ? team_name : "Selected club");
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable upcomingFaTable'><thead><tr>"
        "<th class='roName' data-sort-type='text'>Name</th><th class='roTeam' data-sort-type='text'>Club</th>"
        "<th class='roAge' data-sort-type='number'>Age</th><th data-sort-type='number'>Last FA</th>"
        "<th data-sort-type='number'>Eligible</th><th data-sort-type='number'>Left</th>"
        "<th data-sort-type='number'>Count</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    KboFaRequalificationRecord records[KBO_FA_REQUALIFICATION_MAX];
    int count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    int rendered = 0;
    uint32_t max_year = current_year != 0u ? current_year + 3u : 2200u;

    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < count && rendered < 500; i++) {
            KboFaRequalificationRecord* rec = &records[i];
            uint32_t eligible_year = rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS;
            if (rec->original_team_id != g_kbo_hub_selected_team_id) {
                continue;
            }
            if (current_year != 0u) {
                if (pass == 0 && (eligible_year < current_year || eligible_year > max_year)) {
                    continue;
                }
                if (pass == 1 && eligible_year >= current_year) {
                    continue;
                }
            } else if (pass == 1) {
                continue;
            }

            uint8_t* player = kbo_find_fa_requalification_player_by_id(rec->player_id);
            char player_name[96] = {0};
            uint16_t age = 0u;
            if (player != NULL) {
                kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
                if (memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))) {
                    age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
                }
            }
            if (player_name[0] == '\0') {
                snprintf(player_name, sizeof(player_name), "Player #%u", rec->player_id);
            }

            const char* status = "Scheduled";
            int32_t years_left = 0;
            if (current_year != 0u) {
                years_left = (int32_t)eligible_year - (int32_t)current_year;
                if (years_left <= 0) {
                    status = "Eligible";
                } else if (years_left == 1) {
                    status = "Next year";
                } else {
                    status = "Upcoming";
                }
            }

            kbo_window_text_appendf(buffer, "<tr><td class='roName'>");
            kbo_html_append_escaped(buffer, player_name);
            kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
            kbo_html_append_escaped(buffer, team_name[0] != '\0' ? team_name : "-");
            if (age > 0u) {
                kbo_window_text_appendf(buffer, "</td><td class='roAge'>%u</td>", (uint32_t)age);
            } else {
                kbo_window_text_appendf(buffer, "</td><td class='roAge'></td>");
            }
            kbo_window_text_appendf(
                buffer,
                "<td>%u</td><td>%u</td><td>%d</td><td>%u</td><td class='roStatus'>",
                rec->last_fa_year,
                eligible_year,
                years_left,
                rec->fa_count);
            kbo_html_append_escaped(buffer, status);
            kbo_window_text_appendf(buffer, "</td></tr>");
            rendered++;
        }
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8'>No FA requalification records for the selected club.</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
