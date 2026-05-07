static void kbo_webview_append_asian_quota_view(KboWindowTextBuffer* buffer)
{
    int configured_nations = kbo_load_asian_quota_nation_ids_once();
    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    kbo_count_team_asian_quota_probe(g_kbo_hub_selected_team_id, &team_foreign, &team_asian, &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);

    char summary_text[256] = {0};
    snprintf(
        summary_text,
        sizeof(summary_text),
        "View: Foreign Players - Raw %u / Asian %u / Non-Asian %u / Effective %u - AQ Nations: %d",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective,
        configured_nations);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable foreignRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roSlot' data-sort-type='text'>Slot</th><th class='roTeam' data-sort-type='text'>Team</th>"
        "<th class='roNat' data-sort-type='text'>Nationality*</th><th class='roAge' data-sort-type='number'>Age</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    int rendered = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        for (int32_t i = 0; i < player_count && rendered < 500; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) { continue; }
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
            if (player_id == 0u
                    || !kbo_player_is_foreign_for_kbo_rights(player)
                    || !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id)) {
                continue;
            }

            char player_name[96] = {0};
            char uniform_number[8] = {0};
            char current_team_abbrev[16] = {0};
            char flags[96] = {0};
            uint16_t age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
                ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
                : 0u;
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            kbo_webview_copy_player_uniform_number(player_id, uniform_number, sizeof(uniform_number));
            kbo_hub_copy_team_abbrev_by_id(current_team_id, current_team_abbrev, sizeof(current_team_abbrev), NULL);
            snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
            if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Active"); }

            kbo_window_text_appendf(
                buffer,
                "<tr><td class='roPo'>%s</td><td class='roNum'>",
                kbo_webview_player_position_label(player, 0u));
            kbo_html_append_escaped(buffer, uniform_number);
            kbo_window_text_appendf(buffer, "</td>");
            kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "Unknown player", player_id);
            kbo_window_text_appendf(buffer, "<td class='roSlot'>");
            kbo_html_append_escaped(buffer, kbo_hub_foreign_slot_code_for_player(player));
            kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
            kbo_html_append_escaped(buffer, current_team_abbrev[0] != '\0' ? current_team_abbrev : "-");
            kbo_window_text_appendf(buffer, "</td>");
            kbo_webview_append_roster_nation_cell(buffer, nation_id);
            if (age > 0u) {
                kbo_window_text_appendf(buffer, "<td class='roAge'>%u</td><td class='roStatus'>", (uint32_t)age);
            } else {
                kbo_window_text_appendf(buffer, "<td class='roAge'></td><td class='roStatus'>");
            }
            kbo_html_append_escaped(buffer, flags);
            kbo_window_text_appendf(buffer, "</td></tr>");
            rendered++;
        }
    }
    if (rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
