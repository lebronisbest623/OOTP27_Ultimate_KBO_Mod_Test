static void kbo_webview_append_fa_compensation_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        kbo_window_text_appendf(buffer, "<div class='rights rosterRights'><section class='tablewrap rosterTableWrap'>Could not allocate compensation buffer.</section></div>");
        return;
    }

    char path[MAX_PATH] = {0};
    int count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCompensation'>");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable faCompTable'><thead><tr>"
        "<th data-sort-type='number'>Date</th>"
        "<th class='roName' data-sort-type='text'>Name</th>"
        "<th data-sort-type='number'>Grade</th>"
        "<th data-sort-type='text'>From</th>"
        "<th data-sort-type='text'>To</th>"
        "<th class='roEntry' data-sort-type='number'>Prev Salary</th>"
        "<th class='roEntry' data-sort-type='number'>Cash + Player</th>"
        "<th class='roEntry' data-sort-type='number'>Cash Only</th>"
        "<th data-sort-type='number'>Protected</th>"
        "<th data-sort-type='text'>Status</th>"
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
        kbo_hub_copy_team_abbrev_by_id(rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only, sizeof(cash_only));

        kbo_window_text_appendf(buffer, "<tr>");
        kbo_window_text_appendf(buffer, "<td data-sort-value='%u'>%u</td>", rec->signed_on_yyyymmdd, rec->signed_on_yyyymmdd);
        kbo_webview_append_player_name_cell(buffer, rec->player_name, rec->player_id);
        kbo_window_text_appendf(buffer, "<td data-sort-value='%u'>", grade_sort_rank);
        kbo_html_append_escaped(buffer, grade_display);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, signing_team);
        if (rec->previous_salary > 0) {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%d'>", rec->previous_salary);
        } else {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry'>");
        }
        kbo_html_append_escaped(buffer, previous_salary);
        if (rec->cash_with_player > 0u) {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%u'>", rec->cash_with_player);
        } else {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry'>");
        }
        kbo_html_append_escaped(buffer, cash_with_player);
        if (rec->cash_only > 0u) {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%u'>", rec->cash_only);
        } else {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry'>");
        }
        kbo_html_append_escaped(buffer, cash_only);
        if (rec->protect_count > 0u) {
            kbo_window_text_appendf(buffer, "</td><td data-sort-value='%u'>%u</td><td>", rec->protect_count, rec->protect_count);
        } else {
            kbo_window_text_appendf(buffer, "</td><td data-sort-value='999'>-</td><td>");
        }
        kbo_html_append_escaped(buffer, kbo_fa_compensation_status_label(rec->status));
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='10'>No KBO FA compensation obligations recorded.</td></tr>");
    } else if (count > 800) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='10'>Output truncated. Open fa_compensation.csv for the full ledger.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    HeapFree(GetProcessHeap(), 0, records);
}
