static void kbo_webview_append_asian_games_view(KboWindowTextBuffer* buffer)
{
            kbo_clear_asian_games_roster_if_save_changed("hotkey_ui");
            LONG roster_count = g_kbo_asian_games_roster_count;
            if (roster_count <= 0) {
                kbo_load_asian_games_roster_csv("hotkey_ui");
                roster_count = g_kbo_asian_games_roster_count;
            }
            if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
                roster_count = 0;
            }
            int departed = 0;
            int returned = 0;
            int exempted = 0;
            for (LONG i = 0; i < roster_count; i++) {
                if (g_kbo_asian_games_roster[i].departed) { departed++; }
                if (g_kbo_asian_games_roster[i].returned) { returned++; }
                if (g_kbo_asian_games_roster[i].exempted) { exempted++; }
            }
            kbo_window_text_appendf(buffer, "<div class='rights'><div class='reportbar'><div><b>KBO ASIAN GAMES ROSTER</b><div class='muted'>Year: %u. Selected: %ld / Departed: %d / Returned: %d / Exempted: %d.</div></div><div class='help'><span>CSV roster</span><span>F5 refresh</span></div></div>",
                g_kbo_asian_games_roster_year, roster_count, departed, returned, exempted);
            kbo_window_text_appendf(buffer, "<section class='tablewrap'><table><thead><tr><th></th><th>PLAYER</th><th>TEAM</th><th>AGE</th><th>ROLE</th><th>WC</th><th>STATUS</th></tr></thead><tbody>");
            if (roster_count == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='7'></td></tr>");
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
                    char team_abbrev[16] = {0};
                    kbo_hub_copy_team_abbrev_by_id(entry->original_team_id, team_abbrev, sizeof(team_abbrev), NULL);
                    const char* status = "Selected";
                    if (entry->returned) {
                        status = entry->exempted ? "Returned/Exempt" : "Returned";
                    } else if (entry->departed) {
                        status = "Departed/IL";
                    }
                    kbo_window_text_appendf(buffer, "<tr><td class='sel'>%02ld</td>", i + 1);
                    kbo_webview_append_player_name_cell(buffer, player_name, entry->player_id);
                    kbo_webview_append_team_cell(buffer, team_abbrev);
                    kbo_window_text_appendf(buffer, "<td>%u</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                        (uint32_t)entry->age,
                        kbo_asian_games_role_bucket_label(entry->role),
                        entry->wildcard ? "YES" : "NO",
                        status);
                }
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
