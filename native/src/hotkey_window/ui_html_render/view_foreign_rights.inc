static void kbo_webview_append_foreign_rights_view(KboWindowTextBuffer* buffer, const char* window_status)
{
            kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
            kbo_webview_append_roster_top_bar(buffer, window_status);
            kbo_window_text_appendf(
                buffer,
                "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable foreignRightsTable'><thead><tr>"
                "<th class='roAction'>Rights</th><th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th>"
                "<th class='roName' data-sort-type='text'>Name</th><th class='roTeam' data-sort-type='text'>Team</th>"
                "<th class='roNat' data-sort-type='text'>Nationality*</th><th class='roAge' data-sort-type='number'>Age</th>"
                "<th class='roDate' data-sort-type='date'>Retained</th><th class='roDate' data-sort-type='date'>Expires</th>"
                "<th class='roStatus' data-sort-type='text'>Status</th></tr></thead><tbody>");
    
            uintptr_t player_vector = 0;
            int32_t player_count = 0;
            int rendered = 0;
            uint32_t top_player_id = 0;
            uint32_t top_current_team_id = 0;
            uint32_t window_start = 0u;
            uint32_t window_end = 0u;
            uint32_t today = 0u;
            kbo_current_foreign_waiver_window_dates(&window_start, &window_end);
            kbo_get_foreign_waiver_current_yyyymmdd(&today);
            if (g_kbo_hub_selected_foreign_player_id == 0u
                    && kbo_resolve_foreign_waiver_top_candidate_for_team(g_kbo_hub_selected_team_id, &top_player_id, &top_current_team_id)) {
                g_kbo_hub_selected_foreign_player_id = top_player_id;
            }
            if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
                for (int32_t i = 0; i < player_count && rendered < 500; i++) {
                    uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
                    if (!kbo_player_pointer_plausible(player_ptr)) { continue; }
                    uint8_t* player = (uint8_t*)player_ptr;
                    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                    if (player_id == 0u || today == 0u
                            || !kbo_has_active_foreign_waiver_right(g_kbo_hub_selected_team_id, player_id, today)
                            || !kbo_player_is_foreign_for_kbo_rights(player)) {
                        continue;
                    }
                    char latest_action[16] = {0};
                    if (kbo_foreign_waiver_latest_decision_action(window_end, g_kbo_hub_selected_team_id, player_id, latest_action, sizeof(latest_action))
                            && _stricmp(latest_action, "SKIP") == 0) {
                        continue;
                    }
                    char flags[96] = {0};
                    snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                        player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
                    if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Reserved"); }
                    uint32_t retained_on = 0u;
                    uint32_t expires_on = 0u;
                    kbo_get_active_foreign_waiver_right_dates(
                        g_kbo_hub_selected_team_id,
                        player_id,
                        today,
                        &retained_on,
                        &expires_on);
                    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
                    kbo_webview_append_candidate_card(buffer, player, player_id, g_kbo_hub_selected_team_id, nation_id, flags, retained_on, expires_on);
                    rendered++;
                }
            }
            if (rendered == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='10'></td></tr>");
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
