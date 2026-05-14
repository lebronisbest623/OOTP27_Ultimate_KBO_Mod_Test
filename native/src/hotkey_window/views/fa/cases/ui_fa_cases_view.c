#include "../ui_fa_views_internal.h"
#include "../../../../team/lookup/team_lookup.h"

#define KBO_FA_MARKET_UI_PAGE_SIZE 300

extern int g_kbo_hub_fa_market_page;

static int kbo_fa_market_ui_page_count(int total_rows)
{
    if (total_rows <= 0) {
        return 1;
    }
    return (total_rows + KBO_FA_MARKET_UI_PAGE_SIZE - 1) / KBO_FA_MARKET_UI_PAGE_SIZE;
}

static void kbo_webview_append_fa_market_page_button(
    KboWindowTextBuffer* buffer,
    const char* label,
    int page,
    int enabled)
{
    if (enabled) {
        kbo_window_text_appendf(
            buffer,
            "<a class='rightsTextAction' href='kbo://fa-market/page/%d'>%s</a>",
            page,
            label);
    } else {
        kbo_window_text_appendf(
            buffer,
            "<span class='rightsTextAction' style='opacity:.38;cursor:default'>%s</span>",
            label);
    }
}

void kbo_webview_append_fa_cases_view(KboWindowTextBuffer* buffer, uint32_t selected_league_id)
{
    if (buffer == NULL) {
        return;
    }
    KBO_PROFILE_BEGIN(profile_fa_cases_view);

    static KboFaMarketClassification s_cached_rows[KBO_FA_MARKET_UI_PAGE_SIZE];
    static KboFaMarketScanSummary s_cached_summary;
    static uint32_t s_cached_league_id = 0u;
    static uint32_t s_cached_today = 0u;
    static uint32_t s_cached_year = 0u;
    static int s_cached_page = -1;
    static int s_cached_count = -1;

    uint32_t today = 0u;
    uint32_t current_year = 0u;
    kbo_get_current_yyyymmdd(&today);
    kbo_current_year_relaxed(&current_year);

    int page = g_kbo_hub_fa_market_page;
    if (page < 0) {
        page = 0;
        g_kbo_hub_fa_market_page = 0;
    }

    KboFaMarketScanSummary summary = {0};
    int count = 0;
    int cache_hit = 0;
    if (s_cached_count >= 0
            && s_cached_league_id == selected_league_id
            && s_cached_today == today
            && s_cached_year == current_year
            && s_cached_page == page) {
        count = s_cached_count;
        summary = s_cached_summary;
        cache_hit = 1;
    } else {
        KBO_PROFILE_BEGIN(profile_fa_cases_collect);
        int row_offset = page * KBO_FA_MARKET_UI_PAGE_SIZE;
        count = kbo_collect_fa_market_classifications_page(
            selected_league_id,
            s_cached_rows,
            KBO_FA_MARKET_UI_PAGE_SIZE,
            row_offset,
            &summary,
            0,
            "f2_webview_page");
        if (summary.candidates > 0 && row_offset >= summary.candidates && page > 0) {
            page = kbo_fa_market_ui_page_count(summary.candidates) - 1;
            if (page < 0) {
                page = 0;
            }
            g_kbo_hub_fa_market_page = page;
            row_offset = page * KBO_FA_MARKET_UI_PAGE_SIZE;
            count = kbo_collect_fa_market_classifications_page(
                selected_league_id,
                s_cached_rows,
                KBO_FA_MARKET_UI_PAGE_SIZE,
                row_offset,
                &summary,
                0,
                "f2_webview_page");
        }
        s_cached_league_id = selected_league_id;
        s_cached_today = today;
        s_cached_year = current_year;
        s_cached_page = page;
        s_cached_count = count;
        s_cached_summary = summary;
        KBO_PROFILE_END(profile_fa_cases_collect, "webview.fa_market.collect");
    }

    KBO_PROFILE_BEGIN(profile_fa_cases_render);
    int total_rows = summary.candidates > 0 ? summary.candidates : count;
    int page_count = kbo_fa_market_ui_page_count(total_rows);
    int row_offset = page * KBO_FA_MARKET_UI_PAGE_SIZE;
    int start_row = total_rows > 0 ? row_offset + 1 : 0;
    int end_row = row_offset + count;
    if (end_row > total_rows) {
        end_row = total_rows;
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCases'>");
    kbo_window_text_appendf(
        buffer,
        "<div class='rosterTopBar'><div class='rosterTopText'>FA Market %d-%d / %d</div><div class='rosterTopControls'>",
        start_row,
        end_row,
        total_rows);
    kbo_webview_append_fa_market_page_button(buffer, "First", 0, page > 0);
    kbo_webview_append_fa_market_page_button(buffer, "Prev", page - 1, page > 0);
    kbo_window_text_appendf(buffer, "<span class='rosterTopLabel'>PAGE %d / %d</span>", page + 1, page_count);
    kbo_webview_append_fa_market_page_button(buffer, "Next", page + 1, page + 1 < page_count);
    kbo_webview_append_fa_market_page_button(buffer, "Last", page_count - 1, page + 1 < page_count);
    kbo_window_text_appendf(buffer, "</div></div>");

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

    for (int i = 0; i < count; i++) {
        KboFaMarketClassification* row = &s_cached_rows[i];
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
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    KBO_PROFILE_END(profile_fa_cases_render, cache_hit
        ? "webview.fa_market.render.cache_hit"
        : "webview.fa_market.render.fresh");
    KBO_PROFILE_END(profile_fa_cases_view, cache_hit
        ? "webview.fa_market.total.cache_hit"
        : "webview.fa_market.total.fresh");
}
