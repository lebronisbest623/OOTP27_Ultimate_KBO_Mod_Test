static void kbo_webview_append_asian_quota_view(KboWindowTextBuffer* buffer)
{
            int configured_nations = kbo_load_asian_quota_nation_ids_once();
            uint32_t team_foreign = 0u;
            uint32_t team_asian = 0u;
            uint32_t team_non_asian = 0u;
            kbo_count_team_asian_quota_probe(g_kbo_hub_selected_team_id, &team_foreign, &team_asian, &team_non_asian);
            uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);
            kbo_window_text_appendf(buffer, "<div class='rights'><div class='reportbar'><div><b>KBO FOREIGN PLAYER POLICY</b><div class='muted'>Policy: %s. Base effective limit: %u. Asian nation IDs: %d. Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u.</div></div><div class='help'><span>Selected team</span><span>Asian quota</span><span>F5 refresh</span></div></div>",
                kbo_custom_foreign_policy_enabled() ? "Custom KBO layer" : "OOTP fallback",
                KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
                configured_nations,
                team_foreign,
                team_asian,
                team_non_asian,
                team_effective);
            kbo_window_text_appendf(buffer, "<section class='tablewrap'><table><thead><tr><th>SLOT</th><th>PLAYER</th><th>TEAM</th><th></th><th>STATUS</th></tr></thead><tbody>");
    
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
                    char current_team_abbrev[16] = {0};
                    char flags[96] = {0};
                    kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
                    kbo_hub_copy_team_abbrev_by_id(current_team_id, current_team_abbrev, sizeof(current_team_abbrev), NULL);
                    snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                        player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
                    if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Active"); }
    
                    kbo_window_text_appendf(buffer, "<tr><td class='sel'>");
                    kbo_html_append_escaped(buffer, kbo_hub_foreign_slot_code_for_player(player));
                    kbo_window_text_appendf(buffer, "</td>");
                    kbo_webview_append_player_name_cell(buffer, player_name, player_id);
                    kbo_webview_append_team_cell(buffer, current_team_abbrev);
                    kbo_webview_append_nation_flag_cell(buffer, nation_id);
                    kbo_window_text_appendf(buffer, "<td>");
                    kbo_html_append_escaped(buffer, flags);
                    kbo_window_text_appendf(buffer, "</td></tr>");
                    rendered++;
                }
            }
            if (rendered == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='5'></td></tr>");
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
