#include "ui_fa_cases_view_internal.h"
#include "../../../../team/lookup/team_lookup.h"

void kbo_webview_append_fa_cases_view(KboWindowTextBuffer* buffer, uint32_t selected_league_id)
{
    if (buffer == NULL) {
        return;
    }
    KBO_PROFILE_BEGIN(profile_fa_cases_view);

    static KboFaMarketClassification s_cached_rows[KBO_FA_MARKET_UI_MAX_ROWS];
    static KboFaMarketScanSummary s_cached_summary;
    static uint32_t s_cached_league_id = 0u;
    static uint32_t s_cached_today = 0u;
    static uint32_t s_cached_year = 0u;
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
            && s_cached_year == current_year) {
        count = s_cached_count;
        summary = s_cached_summary;
        cache_hit = 1;
    } else {
        KBO_PROFILE_BEGIN(profile_fa_cases_collect);
        count = kbo_collect_fa_market_classifications_page(
            selected_league_id,
            s_cached_rows,
            KBO_FA_MARKET_UI_MAX_ROWS,
            0,
            &summary,
            0,
            "f2_webview_filter_pool");
        s_cached_league_id = selected_league_id;
        s_cached_today = today;
        s_cached_year = current_year;
        s_cached_count = count;
        s_cached_summary = summary;
        KBO_PROFILE_END(profile_fa_cases_collect, "webview.fa_market.collect");
    }

    KBO_PROFILE_BEGIN(profile_fa_cases_render);
    int filtered_indices[KBO_FA_MARKET_UI_MAX_ROWS];
    int filtered_count = 0;
    for (int i = 0; i < count && filtered_count < KBO_FA_MARKET_UI_MAX_ROWS; i++) {
        if (kbo_fa_market_row_matches_filter(&s_cached_rows[i], g_kbo_hub_fa_market_filter)
                && kbo_fa_market_row_matches_position_filter(
                    &s_cached_rows[i],
                    g_kbo_hub_fa_market_position_filter)) {
            filtered_indices[filtered_count++] = i;
        }
    }
    int page_size = g_kbo_hub_fa_market_report_size;
    if (page_size != 100 && page_size != 300 && page_size != 500) {
        page_size = 300;
    }
    int page_count = kbo_fa_market_ui_page_count(filtered_count);
    if (page >= page_count) {
        page = page_count - 1;
        if (page < 0) {
            page = 0;
        }
        g_kbo_hub_fa_market_page = page;
    }
    int row_offset = page * page_size;
    int visible_count = filtered_count - row_offset;
    if (visible_count < 0) {
        visible_count = 0;
    }
    if (visible_count > page_size) {
        visible_count = page_size;
    }
    int start_row = filtered_count > 0 ? row_offset + 1 : 0;
    int end_row = row_offset + visible_count;
    if (end_row > filtered_count) {
        end_row = filtered_count;
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCases'>");
    kbo_webview_append_fa_market_filter_bar(buffer, filtered_count, count);
    kbo_window_text_appendf(
        buffer,
        "<div class='rosterTopBar'><div class='rosterTopText'>FA 시장 %d-%d / %d</div><div class='rosterTopControls'>",
        start_row,
        end_row,
        filtered_count);
    kbo_webview_append_fa_market_page_button(buffer, "처음", 0, page > 0);
    kbo_webview_append_fa_market_page_button(buffer, "이전", page - 1, page > 0);
    kbo_window_text_appendf(buffer, "<span class='rosterTopLabel'>%d / %d쪽</span>", page + 1, page_count);
    kbo_webview_append_fa_market_page_button(buffer, "다음", page + 1, page + 1 < page_count);
    kbo_webview_append_fa_market_page_button(buffer, "끝", page_count - 1, page + 1 < page_count);
    kbo_window_text_appendf(buffer, "</div></div>");

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable faCasesTable'><thead><tr>"
        "<th data-sort-type='text'>포지션</th>"
        "<th class='roName' data-sort-type='text'>선수</th>"
        "<th data-sort-type='text'>유형</th>"
        "<th data-sort-type='number'>등급</th>"
        "<th class='roEntry' data-sort-type='number'>직전 연봉</th>"
        "<th data-sort-type='text'>구단</th>"
        "<th data-sort-type='number'>나이</th>"
        "<th data-sort-type='text'>국적</th>"
        "<th data-sort-type='text'>보류권</th>"
        "</tr></thead><tbody>");

    for (int i = 0; i < visible_count; i++) {
        int source_index = filtered_indices[row_offset + i];
        KboFaMarketClassification* row = &s_cached_rows[source_index];
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
        kbo_window_text_appendf(buffer, "<td>");
        kbo_html_append_escaped(
            buffer,
            kbo_webview_position_label_from_values(row->position_group, row->position_role));
        kbo_window_text_appendf(buffer, "</td>");
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
        kbo_html_append_escaped(buffer, row->foreign_player ? "외국인" : "국내");
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, rights_abbrev);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (visible_count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='9'>현재 필터와 일치하는 FA 선수를 찾지 못했습니다.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    KBO_PROFILE_END(profile_fa_cases_render, cache_hit
        ? "webview.fa_market.render.cache_hit"
        : "webview.fa_market.render.fresh");
    KBO_PROFILE_END(profile_fa_cases_view, cache_hit
        ? "webview.fa_market.total.cache_hit"
        : "webview.fa_market.total.fresh");
}
