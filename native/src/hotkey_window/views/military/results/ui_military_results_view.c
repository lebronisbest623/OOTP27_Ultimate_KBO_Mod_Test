#include "../internal/ui_military_view_internal.h"
#include "../../mod/info/ui_mod_info_views_internal.h"

void kbo_webview_append_military_results_view(KboWindowTextBuffer* buffer, uint32_t* selected_results_year)
{
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    char sang_name[64] = {0};
    char kpb_name[64]  = {0};
    kbo_hub_copy_team_display_name_from_ptr(sang, sang_name, sizeof(sang_name), "Sangmu Baseball Team");
    kbo_hub_copy_team_display_name_from_ptr(kpb,  kpb_name,  sizeof(kpb_name),  "Korean Police Baseball Team");

    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }

    uint16_t years[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS] = {0};
    int year_count = 0;
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id != 0u && candidate->selected != 0u) {
            uint16_t entry_year = candidate->entry_year;
            int exists = 0;
            for (int y = 0; y < year_count; y++) {
                if (years[y] == entry_year) {
                    exists = 1;
                    break;
                }
            }
            if (!exists && year_count < OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
                years[year_count++] = entry_year;
            }
        }
    }
    for (int left = 0; left < year_count; left++) {
        for (int right = left + 1; right < year_count; right++) {
            if (years[right] > years[left]) {
                uint16_t tmp = years[left];
                years[left] = years[right];
                years[right] = tmp;
            }
        }
    }

    uint32_t selected_year = selected_results_year != NULL ? *selected_results_year : 0u;
    int selected_year_found = 0;
    for (int y = 0; y < year_count; y++) {
        if ((uint32_t)years[y] == selected_year) {
            selected_year_found = 1;
            break;
        }
    }
    if (year_count > 0 && !selected_year_found) {
        selected_year = (uint32_t)years[0];
        if (selected_results_year != NULL) { *selected_results_year = selected_year; }
    } else if (year_count == 0 && selected_year == 0u) {
        uint32_t current_year = 0u;
        if (kbo_current_year_relaxed(&current_year) && current_year != 0u) {
            selected_year = current_year;
            if (selected_results_year != NULL) { *selected_results_year = selected_year; }
        }
    }

    int selected_in_year = 0;
    if (selected_year != 0u) {
        for (LONG i = 0; i < count; i++) {
            KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
            if (candidate->player_id != 0u
                    && candidate->selected != 0u
                    && (uint32_t)candidate->entry_year == selected_year) {
                selected_in_year++;
            }
        }
    }

    char summary_text[160] = {0};
    if (selected_year != 0u) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Announcement Results - %u - Accepted: %d",
            selected_year,
            selected_in_year);
    } else {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "View: Announcement Results - Accepted: %d",
            selected_in_year);
    }
    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'><div class='rosterTopText'>");
    kbo_html_append_escaped(buffer, summary_text);
    char selected_year_label[16] = "-";
    if (selected_year != 0u) {
        snprintf(selected_year_label, sizeof(selected_year_label), "%u", selected_year);
    }
    kbo_window_text_appendf(
        buffer,
        "</div><div class='rosterTopControls'><span class='rosterTopLabel'>YEAR:</span>"
        "<div class='rosterYearChoice'>");
    kbo_webview_begin_ootp_choice(buffer, "militaryResultsYearSelect", selected_year_label);
    if (year_count > 0) {
        for (int y = 0; y < year_count; y++) {
            char href[96] = {0};
            char label[16] = {0};
            snprintf(href, sizeof(href), "kbo://military/results/year/%u", (uint32_t)years[y]);
            snprintf(label, sizeof(label), "%u", (uint32_t)years[y]);
            kbo_webview_append_ootp_choice_option(buffer, href, label, (uint32_t)years[y] == selected_year);
        }
    } else {
        if (selected_year != 0u) {
            char href[96] = {0};
            snprintf(href, sizeof(href), "kbo://military/results/year/%u", selected_year);
            kbo_webview_append_ootp_choice_option(buffer, href, selected_year_label, 1);
        } else {
            kbo_webview_append_ootp_choice_option(buffer, "kbo://military/results/year/0", "-", 1);
        }
    }
    kbo_webview_end_ootp_choice(buffer);
    kbo_window_text_appendf(buffer, "</div></div></div>");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable resultRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roClub' data-sort-type='text'>Original Club</th><th class='roLeague' data-sort-type='text'>Service Team</th>"
        "<th class='roReturn' data-sort-type='text'>Return Date</th><th class='roResult' data-sort-type='text'>Result</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    for (LONG i = 0; i < count && rendered < 500; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u
                || candidate->selected == 0u
                || (uint32_t)candidate->entry_year != selected_year) {
            continue;
        }

        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
        }

        char player_name[96] = {0};
        char original_team_name[64] = {0};
        char service_team_name[64] = {0};
        char return_date[16] = "-";
        const char* position_label = "-";
        snprintf(service_team_name, sizeof(service_team_name), "%s", sang_name[0] != '\0' ? sang_name : "Sangmu Baseball Team");

        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
            uint32_t service_team_id = 0;
            const char* service_fallback = NULL;
            if (sang_id != 0 && (current_team_id == sang_id || loan_team_id == sang_id)) {
                service_team_id = sang_id;
                service_fallback = sang_name;
            } else if (kpb_id != 0 && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
                service_team_id = kpb_id;
                service_fallback = kpb_name;
            }
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            if (service_team_id != 0u) {
                kbo_hub_copy_team_display_name_by_id(
                    service_team_id,
                    service_team_name,
                    sizeof(service_team_name),
                    service_fallback);
            }
            position_label = kbo_webview_player_position_label(player, 0u);
            kbo_military_format_yyyymmdd(
                kbo_military_effective_return_yyyymmdd(player),
                return_date,
                sizeof(return_date));
        } else {
            snprintf(player_name, sizeof(player_name), "#%u", candidate->player_id);
        }

        kbo_hub_copy_team_display_name_by_id(candidate->original_team_id, original_team_name, sizeof(original_team_name), NULL);
        if (original_team_name[0] == '\0') {
            snprintf(original_team_name, sizeof(original_team_name), "-");
        }

        kbo_window_text_appendf(buffer, "<tr><td class='roPo'>%s</td>", position_label);
        kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "Unknown player", candidate->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, original_team_name);
        kbo_window_text_appendf(buffer, "</td><td class='roLeague'>");
        kbo_html_append_escaped(buffer, service_team_name);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roReturn'>%s</td><td class='roResult'>Accepted</td></tr>",
            return_date);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}

