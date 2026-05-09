#include "../ui_asian_games_view_internal.h"

void kbo_webview_append_asian_games_view(KboWindowTextBuffer* buffer, int selected_agames_subview)
{
    if (selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS) {
        kbo_webview_append_asian_games_tournaments_view(buffer);
        return;
    }
    if (selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_SCHEDULE) {
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
                    kbo_window_text_appendf(buffer, "</td>");
                    kbo_webview_append_player_name_cell(buffer, player_name, entry->player_id);
                    kbo_window_text_appendf(
                        buffer,
                        "<td class='roLeague'>Korean National Team</td><td class='roAge'>%u</td>",
                        (uint32_t)entry->age
                    );
                    kbo_webview_append_roster_nation_cell(buffer, OOTP27_KBO_KOREA_NATION_ID, kbo_hub_nation_flag_asset_path);
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
