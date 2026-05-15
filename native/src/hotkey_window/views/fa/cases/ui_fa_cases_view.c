#include "../ui_fa_views_internal.h"
#include "../../../../team/lookup/team_lookup.h"

#define KBO_FA_MARKET_UI_MAX_ROWS 2048

extern int g_kbo_hub_fa_market_page;
extern int g_kbo_hub_fa_market_filter;
extern int g_kbo_hub_fa_market_report_size;

enum {
    KBO_FA_MARKET_FILTER_NONE = 0,
    KBO_FA_MARKET_FILTER_OFFICIAL = 1,
    KBO_FA_MARKET_FILTER_COMPENSABLE = 2,
    KBO_FA_MARKET_FILTER_DOMESTIC = 3,
    KBO_FA_MARKET_FILTER_FOREIGN = 4,
    KBO_FA_MARKET_FILTER_GRADE_A = 5,
    KBO_FA_MARKET_FILTER_GRADE_B = 6,
    KBO_FA_MARKET_FILTER_RIGHTS = 7
};

static int kbo_fa_market_ui_page_count(int total_rows)
{
    int page_size = g_kbo_hub_fa_market_report_size;
    if (page_size != 100 && page_size != 300 && page_size != 500) {
        page_size = 300;
    }
    if (total_rows <= 0) {
        return 1;
    }
    return (total_rows + page_size - 1) / page_size;
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

static const char* kbo_fa_market_filter_label(int filter)
{
    switch (filter) {
    case KBO_FA_MARKET_FILTER_OFFICIAL: return "Official FA";
    case KBO_FA_MARKET_FILTER_COMPENSABLE: return "Compensation";
    case KBO_FA_MARKET_FILTER_DOMESTIC: return "Domestic";
    case KBO_FA_MARKET_FILTER_FOREIGN: return "Foreign";
    case KBO_FA_MARKET_FILTER_GRADE_A: return "Grade A";
    case KBO_FA_MARKET_FILTER_GRADE_B: return "Grade B";
    case KBO_FA_MARKET_FILTER_RIGHTS: return "Rights";
    default: return "None";
    }
}

static int kbo_fa_market_row_is_compensable(const KboFaMarketClassification* row)
{
    return row != NULL
        && (strcmp(row->case_label, "KBO_FA_APPROVED") == 0
            || strcmp(row->case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
            || strcmp(row->case_label, "KBO_FA_DEFERRED") == 0
            || strcmp(row->case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0);
}

static int kbo_fa_market_row_matches_filter(const KboFaMarketClassification* row, int filter)
{
    if (row == NULL) {
        return 0;
    }
    switch (filter) {
    case KBO_FA_MARKET_FILTER_OFFICIAL:
        return strcmp(row->case_label, "KBO_FA_APPROVED") == 0;
    case KBO_FA_MARKET_FILTER_COMPENSABLE:
        return kbo_fa_market_row_is_compensable(row);
    case KBO_FA_MARKET_FILTER_DOMESTIC:
        return row->foreign_player == 0u;
    case KBO_FA_MARKET_FILTER_FOREIGN:
        return row->foreign_player != 0u;
    case KBO_FA_MARKET_FILTER_GRADE_A:
        return _stricmp(row->grade, "A") == 0;
    case KBO_FA_MARKET_FILTER_GRADE_B:
        return _stricmp(row->grade, "B") == 0;
    case KBO_FA_MARKET_FILTER_RIGHTS:
        return row->rights_team_id != 0u;
    default:
        return 1;
    }
}

static void kbo_webview_append_fa_market_choice_option(
    KboWindowTextBuffer* buffer,
    const char* href,
    const char* label,
    int selected)
{
    kbo_window_text_appendf(buffer, "<a class='faFilterOption%s' href='", selected ? " selected" : "");
    kbo_html_append_escaped(buffer, href);
    kbo_window_text_appendf(buffer, "'>");
    kbo_html_append_escaped(buffer, label);
    kbo_window_text_appendf(buffer, "</a>");
}

static void kbo_webview_append_fa_market_filter_bar(
    KboWindowTextBuffer* buffer,
    int filtered_rows,
    int total_rows)
{
    int report_size = g_kbo_hub_fa_market_report_size;
    if (report_size != 100 && report_size != 300 && report_size != 500) {
        report_size = 300;
    }
    kbo_window_text_appendf(buffer,
        "<style>"
        ".faFilterBar{height:34px;display:flex;align-items:center;gap:8px;padding:0 7px;background:#1f1f1f;border:1px solid #303030;border-radius:4px;overflow:visible;flex:0 0 auto}"
        ".faFilterSelect{position:relative;width:150px;height:26px;color:#f2f2f2;font-family:var(--ui-font);font-size:13px;font-weight:900;line-height:26px}.faFilterSelect.wide{width:194px}.faFilterSelect.narrow{width:124px}"
        ".faFilterSelect summary{height:26px;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:0 24px 0 9px;border:1px solid #5a5f61;border-radius:3px;background:#252829;list-style:none;cursor:pointer;outline:0;text-transform:uppercase}.faFilterSelect summary::-webkit-details-marker{display:none}.faFilterSelect summary:after{content:'';position:absolute;right:9px;top:10px;border-left:5px solid transparent;border-right:5px solid transparent;border-top:6px solid #e6e6e6}.faFilterSelect[open] summary{background:#303436;border-color:#8a8f91}.faFilterSelect[open] summary:after{border-top:0;border-bottom:6px solid #fff}"
        ".faFilterMenu{position:absolute;left:0;right:0;top:29px;z-index:60;max-height:230px;overflow-y:auto;overflow-x:hidden;padding:3px 0;border:1px solid #42474a;border-radius:3px;background:#242729;box-shadow:0 8px 18px rgba(0,0,0,.55)}.faFilterOption{display:block;height:24px;line-height:24px;padding:0 9px;color:#efefef;text-decoration:none;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.faFilterOption:hover,.faFilterOption.selected{background:#30434b;color:#fff}"
        ".faFilterSpacer{flex:1 1 auto}.faFilterSummary{color:#f2f2f2;font-size:13px;font-weight:900;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        "</style>"
        "<div class='faFilterBar'><details class='faFilterSelect'><summary>FILTER : ");
    kbo_html_append_escaped(buffer, kbo_fa_market_filter_label(g_kbo_hub_fa_market_filter));
    kbo_window_text_appendf(buffer, "</summary><div class='faFilterMenu'>");
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/0", "None", g_kbo_hub_fa_market_filter == 0);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/1", "Official FA", g_kbo_hub_fa_market_filter == 1);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/2", "Compensation", g_kbo_hub_fa_market_filter == 2);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/3", "Domestic", g_kbo_hub_fa_market_filter == 3);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/4", "Foreign", g_kbo_hub_fa_market_filter == 4);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/5", "Grade A", g_kbo_hub_fa_market_filter == 5);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/6", "Grade B", g_kbo_hub_fa_market_filter == 6);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/7", "Rights", g_kbo_hub_fa_market_filter == 7);
    kbo_window_text_appendf(buffer,
        "</div></details><details class='faFilterSelect narrow'><summary>REPORT</summary><div class='faFilterMenu'>");
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/report/100", "100 Players", report_size == 100);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/report/300", "300 Players", report_size == 300);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/report/500", "500 Players", report_size == 500);
    kbo_window_text_appendf(buffer,
        "</div></details><div class='faFilterSpacer'></div><div class='faFilterSummary'>Filter: ");
    kbo_html_append_escaped(buffer, kbo_fa_market_filter_label(g_kbo_hub_fa_market_filter));
    kbo_window_text_appendf(buffer, " - %d / %d Players</div></div>", filtered_rows, total_rows);
}

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
        if (kbo_fa_market_row_matches_filter(&s_cached_rows[i], g_kbo_hub_fa_market_filter)) {
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
        kbo_window_text_appendf(buffer, "<tr><td colspan='8'>현재 소속팀이 없는 활성 선수를 찾지 못했습니다.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    KBO_PROFILE_END(profile_fa_cases_render, cache_hit
        ? "webview.fa_market.render.cache_hit"
        : "webview.fa_market.render.fresh");
    KBO_PROFILE_END(profile_fa_cases_view, cache_hit
        ? "webview.fa_market.total.cache_hit"
        : "webview.fa_market.total.fresh");
}
