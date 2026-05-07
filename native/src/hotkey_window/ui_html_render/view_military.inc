static void kbo_webview_append_military_roster_view(KboWindowTextBuffer* buffer)
{
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    char sang_name[64] = {0};
    char kpb_name[64]  = {0};
    kbo_hub_copy_team_display_name_from_ptr(sang, sang_name, sizeof(sang_name), "Sangmu Baseball Team");
    kbo_hub_copy_team_display_name_from_ptr(kpb,  kpb_name,  sizeof(kpb_name),  "Korean Police Baseball Team");

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, NULL);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable serviceRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roLeague' data-sort-type='text'>League</th><th class='roClub' data-sort-type='text'>Original Club</th><th class='roReturn' data-sort-type='text'>Return Date</th></tr></thead><tbody>");

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
            uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
            const char* service_fallback = NULL;
            uint32_t service_team_id = 0;
            if (sang_id != 0 && (current_team_id == sang_id || loan_team_id == sang_id)) {
                service_team_id  = sang_id;
                service_fallback = sang_name;
            } else if (kpb_id != 0 && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
                service_team_id  = kpb_id;
                service_fallback = kpb_name;
            }
            if (service_team_id == 0) { continue; }

            uint32_t original_team_id = 0u;
            uint32_t original_league_id = 0u;
            kbo_military_resolve_original_team(
                player,
                service_team_id,
                sang_id,
                kpb_id,
                &original_team_id,
                &original_league_id);
            kbo_military_repair_original_team_memory(
                player,
                original_team_id,
                original_league_id,
                service_team_id,
                sang_id,
                kpb_id);

            char player_name[96] = {0};
            char uniform_number[8] = {0};
            char service_team_name[64] = {0};
            char original_team_name[64] = {0};
            char return_date[16] = {0};
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            kbo_webview_copy_player_uniform_number(player_id, uniform_number, sizeof(uniform_number));
            kbo_hub_copy_team_display_name_by_id(service_team_id,  service_team_name,  sizeof(service_team_name),  service_fallback);
            kbo_hub_copy_team_display_name_by_id(original_team_id, original_team_name, sizeof(original_team_name), NULL);
            kbo_military_format_yyyymmdd(
                kbo_military_effective_return_yyyymmdd(player),
                return_date,
                sizeof(return_date));

            kbo_window_text_appendf(
                buffer,
                "<tr><td class='roPo'>%s</td><td class='roNum'>",
                kbo_webview_player_position_label(player, 0u));
            kbo_html_append_escaped(buffer, uniform_number);
            kbo_window_text_appendf(buffer, "</td><td class='roName'>");
            kbo_html_append_escaped(buffer, player_name[0] != '\0' ? player_name : "Unknown player");
            kbo_window_text_appendf(buffer, "</td><td class='roLeague'>");
            kbo_html_append_escaped(buffer, service_team_name);
            kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
            kbo_html_append_escaped(buffer, original_team_name);
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roReturn'>%s</td></tr>",
                return_date);
            rendered++;
        }
        if (rendered == 0) {
            kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
        }
    } else {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}

static int kbo_military_resolve_application_window(
    uint32_t* out_today,
    uint32_t* out_anchor,
    uint32_t* out_announcement)
{
    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || today == 0u) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }

    uint32_t anchor = kbo_get_latest_offseason_starts_event(today);
    if (anchor == 0u) {
        anchor = kbo_recent_phase_transition_offseason_anchor(league_id, today);
    }
    if (anchor == 0u
            && g_kbo_custom_event_last_offseason_transition_anchor != 0u
            && g_kbo_custom_event_last_offseason_transition_anchor <= today) {
        anchor = g_kbo_custom_event_last_offseason_transition_anchor;
    }
    if (anchor == 0u
            && g_kbo_foreign_priority_last_scheduled_date != 0u
            && g_kbo_foreign_priority_last_scheduled_date <= today) {
        anchor = g_kbo_foreign_priority_last_scheduled_date;
    }

    uint32_t announcement = kbo_add_one_month_yyyymmdd(anchor);
    if (out_today != NULL) { *out_today = today; }
    if (out_anchor != NULL) { *out_anchor = anchor; }
    if (out_announcement != NULL) { *out_announcement = announcement; }
    if (anchor == 0u || announcement == 0u) {
        return 0;
    }
    return today >= anchor && today <= announcement;
}

static int kbo_military_applicant_position_bucket(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))) {
        return 4;
    }
    switch (player[OOTP27_PLAYER_POSITION_GROUP_OFFSET]) {
    case 1u: return 0;
    case 2u: return 1;
    case 3u:
    case 4u:
    case 5u:
    case 6u: return 2;
    case 7u:
    case 8u:
    case 9u: return 3;
    default: break;
    }
    return 4;
}

static void kbo_military_refresh_applicants_for_hotkey_view(void)
{
    uint32_t entry_year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&entry_year, &month, &day)) {
        kbo_current_year_relaxed(&entry_year);
    }
    if (entry_year < 1982u || entry_year > 2300u) {
        return;
    }

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint32_t sang_id = sang != NULL && memory_range_readable(sang, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET)
        : 0u;
    if (sang_id == 0u) {
        return;
    }
    kbo_refresh_military_selection_candidates_from_memory(
        (uint16_t)entry_year,
        sang_id,
        "hotkey_ui_applicants");
}

static void kbo_webview_append_military_applicant_summary(
    KboWindowTextBuffer* buffer,
    int application_active,
    int pending,
    int pitchers,
    int catchers,
    int infielders,
    int outfielders,
    int other,
    uint32_t anchor,
    uint32_t announcement)
{
    char anchor_text[16] = "-";
    char announcement_text[16] = "-";
    kbo_military_format_yyyymmdd(anchor, anchor_text, sizeof(anchor_text));
    kbo_military_format_yyyymmdd(announcement, announcement_text, sizeof(announcement_text));

    char summary_text[320] = {0};
    snprintf(
        summary_text,
        sizeof(summary_text),
        "View: Applicants - Status: %s - POS: P %d / C %d / INF %d / OF %d / Other %d - %d Applicants - Period: %s to %s",
        application_active ? "Open" : "Closed",
        pitchers,
        catchers,
        infielders,
        outfielders,
        other,
        pending,
        anchor_text,
        announcement_text);
    kbo_webview_append_roster_top_bar(buffer, summary_text);
}

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

        kbo_window_text_appendf(buffer, "<tr><td class='roPo'>%s</td><td class='roName'>", position_label);
        kbo_html_append_escaped(buffer, player_name[0] != '\0' ? player_name : "Unknown player");
        kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
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

static void kbo_webview_append_military_results_view(KboWindowTextBuffer* buffer)
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

    uint32_t selected_year = g_kbo_hub_selected_military_results_year;
    int selected_year_found = 0;
    for (int y = 0; y < year_count; y++) {
        if ((uint32_t)years[y] == selected_year) {
            selected_year_found = 1;
            break;
        }
    }
    if (year_count > 0 && !selected_year_found) {
        selected_year = (uint32_t)years[0];
        g_kbo_hub_selected_military_results_year = selected_year;
    } else if (year_count == 0 && selected_year == 0u) {
        uint32_t current_year = 0u;
        if (kbo_current_year_relaxed(&current_year) && current_year != 0u) {
            selected_year = current_year;
            g_kbo_hub_selected_military_results_year = selected_year;
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
    kbo_window_text_appendf(
        buffer,
        "</div><div class='rosterTopControls'><span class='rosterTopLabel'>YEAR:</span>"
        "<select class='ootpSelect rosterYearSelect' onchange=\"if(this.value){location.href='kbo://military/results/year/'+this.value}\">");
    if (year_count > 0) {
        for (int y = 0; y < year_count; y++) {
            kbo_window_text_appendf(
                buffer,
                "<option value='%u' %s>%u</option>",
                (uint32_t)years[y],
                (uint32_t)years[y] == selected_year ? "selected" : "",
                (uint32_t)years[y]);
        }
    } else {
        if (selected_year != 0u) {
            kbo_window_text_appendf(
                buffer,
                "<option value='%u' selected>%u</option>",
                selected_year,
                selected_year);
        } else {
            kbo_window_text_appendf(buffer, "<option value='' selected>-</option>");
        }
    }
    kbo_window_text_appendf(buffer, "</select></div></div>");
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

        kbo_window_text_appendf(buffer, "<tr><td class='roPo'>%s</td><td class='roName'>", position_label);
        kbo_html_append_escaped(buffer, player_name[0] != '\0' ? player_name : "Unknown player");
        kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
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

static void kbo_webview_append_military_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_RESULTS) {
        kbo_webview_append_military_results_view(buffer);
        return;
    }

    if (g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_APPLICANTS) {
        kbo_webview_append_military_applicants_view(buffer);
        return;
    }

    kbo_webview_append_military_roster_view(buffer);
}
