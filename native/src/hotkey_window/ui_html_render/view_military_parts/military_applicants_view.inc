static void kbo_webview_append_military_applicants_view(KboWindowTextBuffer* buffer)
{
    uint32_t today = 0u;
    uint32_t anchor = 0u;
    uint32_t announcement = 0u;
    int application_active = kbo_military_resolve_application_window(&today, &anchor, &announcement);
    if (application_active) {
        kbo_military_refresh_applicants_for_hotkey_view();
    }

    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }

    int pending = 0;
    int position_counts[5] = {0};
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u) { continue; }
        if (candidate->selected == 0u) {
            pending++;
            uintptr_t player_ptr = candidate->player_ptr;
            if (!kbo_player_pointer_plausible(player_ptr)) {
                player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
            }
            int bucket = kbo_military_applicant_position_bucket(
                kbo_player_pointer_plausible(player_ptr) ? (uint8_t*)player_ptr : NULL);
            if (bucket < 0 || bucket > 4) {
                bucket = 4;
            }
            position_counts[bucket]++;
        }
    }

    (void)today;
    int summary_pending = application_active ? pending : 0;
    int summary_pitchers = application_active ? position_counts[0] : 0;
    int summary_catchers = application_active ? position_counts[1] : 0;
    int summary_infielders = application_active ? position_counts[2] : 0;
    int summary_outfielders = application_active ? position_counts[3] : 0;
    int summary_other = application_active ? position_counts[4] : 0;

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights applicantRights'>");
    kbo_webview_append_military_applicant_summary(
        buffer,
        application_active,
        summary_pending,
        summary_pitchers,
        summary_catchers,
        summary_infielders,
        summary_outfielders,
        summary_other,
        anchor,
        announcement);

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable applicantRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roClub' data-sort-type='text'>Original Club</th><th class='roAge' data-sort-type='number'>Age</th>"
        "<th class='roEntry' data-sort-type='number'>Entry Year</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    if (!application_active) {
        kbo_window_text_appendf(buffer, "<tr><td class='roEmptyMessage' colspan='6'>지원 기간이 아닙니다.</td></tr>");
    }
    for (LONG i = 0; application_active && i < count && rendered < 500; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u) { continue; }
        if (candidate->selected != 0u) { continue; }

        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
        }

        char player_name[96] = {0};
        const char* position_label = "-";
        uint16_t age = 0u;
        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* player = (uint8_t*)player_ptr;
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            position_label = kbo_webview_player_position_label(player, 0u);
            if (memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))) {
                age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
            }
        } else {
            snprintf(player_name, sizeof(player_name), "#%u", candidate->player_id);
        }

        char original_team_name[64] = {0};
        kbo_hub_copy_team_display_name_by_id(candidate->original_team_id, original_team_name, sizeof(original_team_name), NULL);
        if (original_team_name[0] == '\0') {
            snprintf(original_team_name, sizeof(original_team_name), "-");
        }

        kbo_window_text_appendf(buffer, "<tr><td class='roPo'>%s</td>", position_label);
        kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "Unknown player", candidate->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, original_team_name);
        if (age > 0u) {
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roAge'>%u</td><td class='roEntry'>%u</td><td class='roStatus'>Queued</td></tr>",
                (uint32_t)age,
                (uint32_t)candidate->entry_year);
        } else {
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roAge'></td><td class='roEntry'>%u</td><td class='roStatus'>Queued</td></tr>",
                (uint32_t)candidate->entry_year);
        }
        rendered++;
    }

    if (application_active && rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
