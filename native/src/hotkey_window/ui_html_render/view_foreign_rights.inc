static void kbo_webview_append_foreign_rights_view(KboWindowTextBuffer* buffer, const char* window_status)
{
            kbo_window_text_appendf(buffer, "<div class='rights'><div class='reportbar'><div><b>KBO FOREIGN RESERVE RIGHTS</b><div class='muted'>");
            kbo_html_append_escaped(buffer, window_status);
            kbo_window_text_appendf(buffer, "</div></div><div class='help'><span>&#xD589;&#xC0AC;/&#xBBF8;&#xD589;&#xC0AC;</span><span>F5 refresh</span></div></div>");
            kbo_window_text_appendf(buffer, "<section class='tablewrap'><table><thead><tr><th>RIGHTS</th><th>PLAYER</th><th>TEAM</th><th></th><th>STATUS</th></tr></thead><tbody>");
    
            uintptr_t player_vector = 0;
            int32_t player_count = 0;
            int rendered = 0;
            uint32_t top_player_id = 0;
            uint32_t top_current_team_id = 0;
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
                    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                    uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
                    if (player_id == 0u || decision_team_id != g_kbo_hub_selected_team_id
                            || !kbo_player_is_foreign_for_kbo_rights(player)) {
                        continue;
                    }
                    char flags[96] = {0};
                    snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                        player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
                    if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Active"); }
                    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
                    kbo_webview_append_candidate_card(buffer, player, player_id, current_team_id, nation_id, flags);
                    rendered++;
                }
            }
            if (rendered == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='5'></td></tr>");
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
