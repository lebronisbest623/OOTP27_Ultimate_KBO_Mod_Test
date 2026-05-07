static int kbo_fa_compensation_record_is_final(const KboFaCompensationRecord* rec)
{
    if (rec == NULL) {
        return 0;
    }
    return rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED
        || rec->status == KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED;
}

static int kbo_fa_compensation_debug_row_count(
    const KboFaCompensationProtectionDebugRow* rows,
    int count,
    uint32_t fa_player_id)
{
    int row_count = 0;
    if (rows == NULL || count <= 0 || fa_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (rows[i].fa_player_id == fa_player_id) {
            row_count++;
        }
    }
    return row_count;
}

static const char* kbo_fa_compensation_decision_label(
    const KboFaCompensationRecord* rec,
    const KboFaCompensationDecisionRow* decision,
    int has_decision)
{
    if (has_decision && decision != NULL) {
        if (strcmp(decision->action, "CASH_ONLY") == 0) {
            return "Cash Only";
        }
        if (decision->selected_player_name[0] != '\0') {
            return decision->selected_player_name;
        }
        return "Player + Cash";
    }
    if (rec != NULL && rec->requires_player_compensation && rec->protect_count > 0u) {
        return "Awaiting list";
    }
    return "Cash Only";
}

static void kbo_webview_append_fa_compensation_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaCompensationProtectionDebugRow* debug_rows = (KboFaCompensationProtectionDebugRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaCompensationProtectionDebugRow));
    if (records == NULL || debug_rows == NULL) {
        kbo_window_text_appendf(buffer, "<div class='rights rosterRights'><section class='tablewrap rosterTableWrap'>Could not allocate compensation buffer.</section></div>");
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (debug_rows != NULL) { HeapFree(GetProcessHeap(), 0, debug_rows); }
        return;
    }

    char path[MAX_PATH] = {0};
    int count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int debug_count = kbo_load_fa_compensation_protection_debug_rows(debug_rows, KBO_FA_COMPENSATION_PROTECTED_LIST_MAX);
    KboFaCompensationRecord* detail_rec = NULL;

    if (g_kbo_hub_selected_fa_compensation_player_id != 0u) {
        for (int i = 0; i < count; i++) {
            if (records[i].player_id == g_kbo_hub_selected_fa_compensation_player_id
                    && records[i].requires_player_compensation
                    && records[i].protect_count > 0u) {
                detail_rec = &records[i];
                break;
            }
        }
    }
    for (int i = 0; i < count && detail_rec == NULL; i++) {
        if (records[i].requires_player_compensation && records[i].protect_count > 0u
                && !kbo_fa_compensation_record_is_final(&records[i])) {
            detail_rec = &records[i];
        }
    }
    for (int i = 0; i < count && detail_rec == NULL; i++) {
        if (records[i].requires_player_compensation && records[i].protect_count > 0u) {
            detail_rec = &records[i];
        }
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCompensation'>");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap faCompLedgerWrap'><table class='ootpRosterTable faCompTable'><thead><tr>"
        "<th data-sort-type='number'>Date</th>"
        "<th class='roName' data-sort-type='text'>FA</th>"
        "<th data-sort-type='number'>Grade</th>"
        "<th data-sort-type='text'>From</th>"
        "<th data-sort-type='text'>To</th>"
        "<th class='roEntry' data-sort-type='number'>Prev Salary</th>"
        "<th class='roEntry' data-sort-type='number'>Player + Cash</th>"
        "<th class='roEntry' data-sort-type='number'>Cash Only</th>"
        "<th class='roStatus' data-sort-type='text'>Status</th>"
        "<th class='roAction' data-sort-type='text'>Action</th>"
        "</tr></thead><tbody>");

    for (int i = 0; i < count && i < 800; i++) {
        KboFaCompensationRecord* rec = &records[i];
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char previous_salary[32] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";
        const char* grade_display = kbo_fa_market_display_grade(rec->grade);
        uint32_t grade_sort_rank = kbo_fa_market_display_grade_sort_rank(rec->grade);
        int row_selected = detail_rec != NULL && detail_rec->player_id == rec->player_id;

        kbo_hub_copy_team_abbrev_by_id(rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only, sizeof(cash_only));

        kbo_window_text_appendf(buffer, row_selected ? "<tr class='selected'>" : "<tr>");
        kbo_window_text_appendf(buffer, "<td data-sort-value='%u'>%u</td>", rec->signed_on_yyyymmdd, rec->signed_on_yyyymmdd);
        kbo_webview_append_player_name_cell(buffer, rec->player_name, rec->player_id);
        kbo_window_text_appendf(buffer, "<td data-sort-value='%u'>", grade_sort_rank);
        kbo_html_append_escaped(buffer, grade_display);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, signing_team);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%d'>", rec->previous_salary);
        kbo_html_append_escaped(buffer, previous_salary);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%u'>", rec->cash_with_player);
        kbo_html_append_escaped(buffer, cash_with_player);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%u'>", rec->cash_only);
        kbo_html_append_escaped(buffer, cash_only);
        kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
        kbo_html_append_escaped(buffer, kbo_fa_compensation_status_label(rec->status));
        kbo_window_text_appendf(buffer, "</td><td class='roAction'>");

        if (rec->requires_player_compensation && rec->protect_count > 0u) {
            if (rec->status == KBO_FA_COMPENSATION_STATUS_PENDING
                    || rec->status == KBO_FA_COMPENSATION_STATUS_RECORDED) {
                kbo_window_text_appendf(buffer, "<a class='rightsTextAction' title='Submit protected list' href='kbo://fa-comp/submit/%u'>Submit</a>", rec->player_id);
            } else {
                kbo_window_text_appendf(buffer, "<a class='rightsTextAction' title='Open compensation board' href='kbo://fa-comp/detail/%u'>Open</a>", rec->player_id);
            }
        } else {
            kbo_html_append_escaped(buffer, "-");
        }
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='10' class='roEmptyMessage'>No KBO FA compensation obligations recorded.</td></tr>");
    } else if (count > 800) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='10' class='roEmptyMessage'>Output truncated. Open fa_compensation.csv for the full ledger.</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");

    if (detail_rec != NULL) {
        KboFaCompensationDecisionRow decision;
        int has_decision = kbo_load_latest_fa_compensation_decision(detail_rec->player_id, &decision);
        int detail_rows = kbo_fa_compensation_debug_row_count(debug_rows, debug_count, detail_rec->player_id);
        int is_final = kbo_fa_compensation_record_is_final(detail_rec);
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";

        kbo_hub_copy_team_abbrev_by_id(detail_rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(detail_rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary((int32_t)detail_rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)detail_rec->cash_only, cash_only, sizeof(cash_only));

        kbo_window_text_appendf(buffer, "<div class='faCompDecisionBar'>");
        kbo_window_text_appendf(buffer, "<div class='faCompDecisionMain'>");
        kbo_html_append_escaped(buffer, detail_rec->player_name);
        kbo_window_text_appendf(buffer, "<span class='faCompBadge'>");
        kbo_html_append_escaped(buffer, kbo_fa_market_display_grade(detail_rec->grade));
        kbo_window_text_appendf(buffer, "</span><span class='faCompRoute'>");
        kbo_html_append_escaped(buffer, signing_team);
        kbo_window_text_appendf(buffer, " -> ");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</span></div>");

        kbo_window_text_appendf(buffer, "<div class='faCompDecisionMeta'><span>Decision: ");
        kbo_html_append_escaped(buffer, kbo_fa_compensation_decision_label(detail_rec, &decision, has_decision));
        kbo_window_text_appendf(buffer, "</span><span>Player + Cash: ");
        kbo_html_append_escaped(buffer, cash_with_player);
        kbo_window_text_appendf(buffer, "</span><span>Cash Only: ");
        kbo_html_append_escaped(buffer, cash_only);
        kbo_window_text_appendf(buffer, "</span><span>Protected: %u</span></div>", detail_rec->protect_count);

        kbo_window_text_appendf(buffer, "<div class='faCompDecisionActions'>");
        if (is_final) {
            kbo_window_text_appendf(buffer, "<span class='faCompFinal'>Final</span>");
        } else if (detail_rows <= 0) {
            kbo_window_text_appendf(buffer, "<a class='rightsTextAction' href='kbo://fa-comp/submit/%u'>Submit List</a>", detail_rec->player_id);
        } else {
            kbo_window_text_appendf(buffer, "<a class='rightsTextAction cashOnly' href='kbo://fa-comp/cash-only/%u'>Cash Only</a>", detail_rec->player_id);
        }
        kbo_window_text_appendf(buffer, "</div></div>");

        kbo_window_text_appendf(
            buffer,
            "<section class='tablewrap rosterTableWrap faCompLists'><table class='ootpRosterTable faCompListTable'><thead><tr>"
            "<th class='faCompPool' data-sort-type='text'>Pool</th>"
            "<th class='faCompRank' data-sort-type='number'>Rank</th>"
            "<th class='roName' data-sort-type='text'>Player</th>"
            "<th class='faCompAge' data-sort-type='number'>Age</th>"
            "<th class='faCompScore' data-sort-type='number'>Score</th>"
            "<th class='roAction' data-sort-type='text'>Decision</th>"
            "</tr></thead><tbody>");

        int rendered = 0;
        static const char* list_order[] = { "auto_protected", "protected", "unprotected" };
        for (int list_index = 0; list_index < 3; list_index++) {
            const char* list_type = list_order[list_index];
            for (int i = 0; i < debug_count; i++) {
                KboFaCompensationProtectionDebugRow* row = &debug_rows[i];
                if (row->fa_player_id != detail_rec->player_id || strcmp(row->list_type, list_type) != 0) {
                    continue;
                }
                const char* label = strcmp(row->list_type, "auto_protected") == 0
                    ? "Auto" : (strcmp(row->list_type, "protected") == 0 ? "Protected" : "Available");
                int selected_player = has_decision
                    && strcmp(decision.action, "PLAYER") == 0
                    && row->player_id == decision.selected_player_id;

                kbo_window_text_appendf(buffer, selected_player ? "<tr class='selected'>" : "<tr>");
                kbo_window_text_appendf(buffer, "<td class='faCompPool'>");
                kbo_html_append_escaped(buffer, label);
                kbo_window_text_appendf(buffer, "</td><td class='faCompRank' data-sort-value='%u'>%u</td>", row->rank, row->rank);
                kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
                kbo_window_text_appendf(
                    buffer,
                    "<td class='faCompAge' data-sort-value='%u'>%u</td><td class='faCompScore' data-sort-value='%d'>%d</td><td class='roAction'>",
                    (uint32_t)row->age,
                    (uint32_t)row->age,
                    row->score,
                    row->score);
                if (selected_player) {
                    kbo_window_text_appendf(buffer, "<span class='faCompPick'>Selected</span>");
                } else if (!is_final && strcmp(row->list_type, "unprotected") == 0) {
                    kbo_window_text_appendf(
                        buffer,
                        "<a class='rightsTextAction' href='kbo://fa-comp/select/%u/%u'>Select</a>",
                        detail_rec->player_id,
                        row->player_id);
                } else {
                    kbo_html_append_escaped(buffer, "-");
                }
                kbo_window_text_appendf(buffer, "</td></tr>");
                rendered++;
            }
        }
        if (rendered == 0) {
            kbo_window_text_appendf(buffer, "<tr><td colspan='6' class='roEmptyMessage'>Protected list has not been submitted yet.</td></tr>");
        }
        kbo_window_text_appendf(buffer, "</tbody></table></section>");
    } else {
        kbo_window_text_appendf(buffer, "<section class='tablewrap rosterTableWrap faCompLists'><table class='ootpRosterTable'><tbody><tr><td class='roEmptyMessage'>No player compensation board is available.</td></tr></tbody></table></section>");
    }

    kbo_window_text_appendf(buffer, "</div>");
    HeapFree(GetProcessHeap(), 0, debug_rows);
    HeapFree(GetProcessHeap(), 0, records);
}
