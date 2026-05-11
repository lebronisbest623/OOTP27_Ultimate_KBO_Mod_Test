#include "../ui_fa_views_internal.h"

void kbo_webview_append_fa_cases_view(KboWindowTextBuffer* buffer, uint32_t selected_league_id)
{
    if (buffer == NULL) {
        return;
    }

    static KboFaMarketClassification s_cached_rows[KBO_FA_MARKET_CLASSIFICATION_MAX];
    static KboFaMarketScanSummary s_cached_summary;
    static uint32_t s_cached_league_id = 0u;
    static uint32_t s_cached_today = 0u;
    static uint32_t s_cached_year = 0u;
    static int s_cached_count = -1;

    KboFaMarketClassification* rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_CLASSIFICATION_MAX * sizeof(KboFaMarketClassification));
    if (rows == NULL) {
        kbo_window_text_appendf(buffer, "<div class='rights rosterRights'><section class='tablewrap rosterTableWrap'>Could not allocate classification buffer.</section></div>");
        return;
    }

    uint32_t today = 0u;
    uint32_t current_year = 0u;
    kbo_get_current_yyyymmdd(&today);
    kbo_current_year_relaxed(&current_year);

    KboFaMarketScanSummary summary = {0};
    int count = 0;
    if (s_cached_count >= 0
            && s_cached_league_id == selected_league_id
            && s_cached_today == today
            && s_cached_year == current_year) {
        count = s_cached_count;
        summary = s_cached_summary;
        memcpy(rows, s_cached_rows, (SIZE_T)count * sizeof(rows[0]));
    } else {
        count = kbo_collect_fa_market_classifications(
            selected_league_id,
            rows,
            KBO_FA_MARKET_CLASSIFICATION_MAX,
            &summary,
            0,
            "f2_webview");
        s_cached_league_id = selected_league_id;
        s_cached_today = today;
        s_cached_year = current_year;
        s_cached_count = count;
        s_cached_summary = summary;
        memcpy(s_cached_rows, rows, (SIZE_T)count * sizeof(rows[0]));
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCases'>");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable faCasesTable'><thead><tr>"
        "<th class='roName' data-sort-type='text'>Name</th>"
        "<th data-sort-type='text'>Case</th>"
        "<th data-sort-type='number'>Grade</th>"
        "<th class='roEntry' data-sort-type='number'>Prev Salary</th>"
        "<th data-sort-type='text'>Team</th>"
        "<th data-sort-type='number'>Age</th>"
        "<th data-sort-type='text'>Nation</th>"
        "<th data-sort-type='text'>Rights</th>"
        "</tr></thead><tbody>");

    for (int i = 0; i < count && i < 800; i++) {
        KboFaMarketClassification* row = &rows[i];
        char team_abbrev[16] = "-";
        char rights_abbrev[16] = "-";
        char salary_text[32] = "-";
        const char* grade_display = kbo_fa_market_display_grade(row->grade);
        uint32_t grade_sort_rank = kbo_fa_market_display_grade_sort_rank(row->grade);
        kbo_fa_market_format_salary(row->fa_grade_salary, salary_text, sizeof(salary_text));
        kbo_hub_copy_team_abbrev_by_id(
            kbo_fa_market_display_team_id(row),
            team_abbrev,
            sizeof(team_abbrev),
            "-");
        kbo_hub_copy_team_abbrev_by_id(row->rights_team_id, rights_abbrev, sizeof(rights_abbrev), "-");
        kbo_window_text_appendf(buffer, "<tr>");
        kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
        kbo_window_text_appendf(buffer, "<td>");
        kbo_html_append_escaped(buffer, kbo_fa_market_display_case_label(row->case_label));
        kbo_window_text_appendf(buffer, "</td><td data-sort-value='%u'>", grade_sort_rank);
        kbo_html_append_escaped(buffer, grade_display);
        if (row->fa_grade_salary > 0) {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%d'>", row->fa_grade_salary);
        } else {
            kbo_window_text_appendf(buffer, "</td><td class='roEntry'>");
        }
        kbo_html_append_escaped(buffer, salary_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td>");
        kbo_html_append_escaped(buffer, team_abbrev);
        kbo_window_text_appendf(
            buffer,
            "</td><td>%u</td><td>",
            (uint32_t)row->age);
        kbo_html_append_escaped(buffer, row->foreign_player ? "Foreign" : "KOR");
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, rights_abbrev);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8'>No active players without a current team found.</td></tr>");
    } else if (summary.truncated || count > 800) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8'>Output truncated. Open fa_market_classification.csv for the full snapshot.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    HeapFree(GetProcessHeap(), 0, rows);
}
