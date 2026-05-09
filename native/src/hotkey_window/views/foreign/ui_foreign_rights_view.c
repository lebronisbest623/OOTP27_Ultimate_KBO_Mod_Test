#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../foreign/waiver_core/api/foreign_waiver_core.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/rights/query/foreign_waiver_rights_query.h"
#include "../../../military_service/selection/events/military_selection_event.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../support/assets/names/support_names.h"
#include "../../support/assets/paths/ui_asset_paths.h"
#include "ui_foreign_rights_view.h"
#include "../../support/assets/nations/ui_nation_helpers.h"
#include "../../support/roster/cells/ui_roster_cells.h"
#include "../../support/text/buffer/ui_text_buffer.h"
#include "../../support/assets/names/ui_uniform_numbers.h"
#include "../../ui_html_helpers/position_helpers.h"

static void kbo_webview_append_candidate_card(
    KboWindowTextBuffer* buffer,
    uint8_t* player,
    uint32_t player_id,
    uint32_t current_team_id,
    uint32_t nation_id,
    const char* flags,
    uint32_t retained_on,
    uint32_t expires_on,
    uint32_t selected_foreign_player_id)
{
    char player_name[96] = {0};
    char team_abbrev[16] = {0};
    char uniform_number[8] = {0};
    char retained_text[16] = {0};
    char expires_text[16] = {0};
    uint16_t age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
        ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
        : 0u;
    kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
    kbo_hub_copy_team_abbrev_by_id(current_team_id, team_abbrev, sizeof(team_abbrev), NULL);
    kbo_webview_copy_player_uniform_number(player_id, uniform_number, sizeof(uniform_number));
    kbo_military_format_yyyymmdd(retained_on, retained_text, sizeof(retained_text));
    kbo_military_format_yyyymmdd(expires_on, expires_text, sizeof(expires_text));

    kbo_window_text_appendf(
        buffer,
        "<tr%s><td class='roAction'><span class='rightsActions'>"
        "<a class='rightsAction rightsRelease' title='Release reserve rights' href='kbo://release/%u' data-player='",
        player_id == selected_foreign_player_id ? " class='selected'" : "",
        player_id);
    kbo_html_append_escaped(buffer, player_name[0] != '\0' ? player_name : "Unknown player");
    kbo_window_text_appendf(
        buffer,
        "'>-</a></span></td><td class='roPo'>%s</td><td class='roNum'>",
        kbo_webview_player_position_label(player, 0u));
    kbo_html_append_escaped(buffer, uniform_number);
    kbo_window_text_appendf(buffer, "</td>");
    kbo_webview_append_player_name_link_cell(
        buffer,
        player_name[0] != '\0' ? player_name : "Unknown player",
        player_id,
        "kbo://select/");
    kbo_window_text_appendf(buffer, "<td class='roTeam'>");
    kbo_html_append_escaped(buffer, team_abbrev[0] != '\0' ? team_abbrev : "-");
    kbo_window_text_appendf(buffer, "</td>");
    kbo_webview_append_roster_nation_cell(buffer, nation_id, kbo_hub_nation_flag_asset_path);
    if (age > 0u) {
        kbo_window_text_appendf(buffer, "<td class='roAge'>%u</td>", (uint32_t)age);
    } else {
        kbo_window_text_appendf(buffer, "<td class='roAge'></td>");
    }
    kbo_window_text_appendf(buffer, "<td class='roDate'>");
    kbo_html_append_escaped(buffer, retained_text);
    kbo_window_text_appendf(buffer, "</td><td class='roDate'>");
    kbo_html_append_escaped(buffer, expires_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
    kbo_html_append_escaped(buffer, flags != NULL && flags[0] != '\0' ? flags : "Active");
    kbo_window_text_appendf(buffer, "</td></tr>");
}

void kbo_webview_append_foreign_rights_view(
    KboWindowTextBuffer* buffer,
    const char* window_status,
    uint32_t selected_team_id,
    uint32_t* selected_foreign_player_id)
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
            if (selected_foreign_player_id != NULL
                    && *selected_foreign_player_id == 0u
                    && kbo_resolve_foreign_waiver_top_candidate_for_team(selected_team_id, &top_player_id, &top_current_team_id)) {
                *selected_foreign_player_id = top_player_id;
            }
            if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
                for (int32_t i = 0; i < player_count && rendered < 500; i++) {
                    uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
                    if (!kbo_player_pointer_plausible(player_ptr)) { continue; }
                    uint8_t* player = (uint8_t*)player_ptr;
                    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                    if (player_id == 0u || today == 0u
                            || !kbo_has_active_foreign_waiver_right(selected_team_id, player_id, today)
                            || !kbo_player_is_foreign_for_kbo_rights(player)) {
                        continue;
                    }
                    char latest_action[16] = {0};
                    if (kbo_foreign_waiver_latest_decision_action(window_end, selected_team_id, player_id, latest_action, sizeof(latest_action))
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
                        selected_team_id,
                        player_id,
                        today,
                        &retained_on,
                        &expires_on);
                    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
                    kbo_webview_append_candidate_card(
                        buffer,
                        player,
                        player_id,
                        selected_team_id,
                        nation_id,
                        flags,
                        retained_on,
                        expires_on,
                        selected_foreign_player_id != NULL ? *selected_foreign_player_id : 0u);
                    rendered++;
                }
            }
            if (rendered == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='10'></td></tr>");
            }
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
