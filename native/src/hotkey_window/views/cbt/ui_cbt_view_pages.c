#include "ui_cbt_view_internal.h"

void kbo_webview_append_cbt_history_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    KboCbtRecord* records = NULL;
    int count = 0;
    if (!kbo_cbt_load_sorted(&records, &count)) return;

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);

    char team_name[96];
    kbo_hub_copy_team_display_name_by_id(selected_team_id, team_name, sizeof(team_name), "No team");
    char summary[256];
    snprintf(summary, sizeof(summary), "Team history - %s", team_name);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary);
    kbo_window_text_appendf(buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable'><thead><tr>"
        "<th data-sort-type='number' style='width:64px'>Season</th>"
        "<th data-sort-type='number' style='width:92px'>Payroll</th>"
        "<th data-sort-type='number' style='width:92px'>Threshold</th>"
        "<th data-sort-type='number' style='width:92px'>Overage</th>"
        "<th data-sort-type='number' style='width:54px'>Rate</th>"
        "<th data-sort-type='number' style='width:92px'>Tax Paid</th>"
        "<th data-sort-type='number' style='width:64px'>Consec.</th>"
        "<th data-sort-type='text' style='width:92px'>Status</th>"
        "<th data-sort-type='number' style='width:84px'>Draft Pen.</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    for (int i = 0; i < count; i++) {
        const KboCbtRecord* rec = &records[i];
        if (rec->team_id != selected_team_id) continue;
        char payroll[32], threshold[32], overage[32], tax[32];
        int draft = 0;
        const char* row_style = rec->overage > 0 ? " style='color:#e88;'" : "";
        const char* status = rec->overage > 0 ? "Over Line" : "Clear";
        kbo_cbt_format_usd(payroll, sizeof(payroll), rec->payroll);
        kbo_cbt_format_usd(threshold, sizeof(threshold), rec->threshold);
        kbo_cbt_format_usd(overage, sizeof(overage), rec->overage);
        kbo_cbt_format_usd(tax, sizeof(tax), rec->tax_amount);
        if (kbo_cbt_is_latest_for_team(records, i)
            && rec->overage > 0
            && rec->consecutive_count >= rules.draft_penalty_min_consecutive) {
            draft = (int)rules.draft_penalty_stages;
        }

        kbo_window_text_appendf(buffer, "<tr%s><td>%u</td><td>", row_style, rec->season);
        kbo_html_append_escaped(buffer, payroll);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, threshold);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, overage);
        kbo_window_text_appendf(buffer, "</td><td>%u%%</td><td>", rec->tax_rate_pct);
        kbo_html_append_escaped(buffer, tax);
        if (draft > 0) {
            kbo_window_text_appendf(buffer,
                "</td><td>%u</td><td>%s</td><td style='color:#e88;'>-%d</td></tr>",
                rec->consecutive_count, status, draft);
        } else {
            kbo_window_text_appendf(buffer,
                "</td><td>%u</td><td>%s</td><td>-</td></tr>",
                rec->consecutive_count, status);
        }
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer,
            "<tr><td colspan='9' class='roEmptyMessage'>No competitive balance tax history for the selected team.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    HeapFree(GetProcessHeap(), 0, records);
}

static void kbo_cbt_copy_button_asset_src(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) return;
    out[0] = '\0';
    char path[MAX_PATH] = {0};
    kbo_hub_skin_button_image_path(file_name, path, sizeof(path));
    kbo_webview_copy_image_src(path, out, out_size);
}

static void kbo_webview_append_cbt_exception_css(KboWindowTextBuffer* buffer)
{
    char plus_up[2048] = {0}, plus_over[2048] = {0}, plus_down[2048] = {0};
    char minus_up[2048] = {0}, minus_over[2048] = {0}, minus_down[2048] = {0};
    char btn_left[2048] = {0}, btn_mid[2048] = {0}, btn_right[2048] = {0};
    char btn_left_over[2048] = {0}, btn_mid_over[2048] = {0}, btn_right_over[2048] = {0};
    char btn_left_down[2048] = {0}, btn_mid_down[2048] = {0}, btn_right_down[2048] = {0};

    kbo_cbt_copy_button_asset_src("list_buttons_plus_up.png", plus_up, sizeof(plus_up));
    kbo_cbt_copy_button_asset_src("list_buttons_plus_over.png", plus_over, sizeof(plus_over));
    kbo_cbt_copy_button_asset_src("list_buttons_plus_down.png", plus_down, sizeof(plus_down));
    kbo_cbt_copy_button_asset_src("list_buttons_minus_up.png", minus_up, sizeof(minus_up));
    kbo_cbt_copy_button_asset_src("list_buttons_minus_over.png", minus_over, sizeof(minus_over));
    kbo_cbt_copy_button_asset_src("list_buttons_minus_down.png", minus_down, sizeof(minus_down));
    kbo_cbt_copy_button_asset_src("list_button_up_leftr.png", btn_left, sizeof(btn_left));
    kbo_cbt_copy_button_asset_src("list_button_up_mid.png", btn_mid, sizeof(btn_mid));
    kbo_cbt_copy_button_asset_src("list_button_up_rightr.png", btn_right, sizeof(btn_right));
    kbo_cbt_copy_button_asset_src("list_button_over_leftr.png", btn_left_over, sizeof(btn_left_over));
    kbo_cbt_copy_button_asset_src("list_button_over_mid.png", btn_mid_over, sizeof(btn_mid_over));
    kbo_cbt_copy_button_asset_src("list_button_over_rightr.png", btn_right_over, sizeof(btn_right_over));
    kbo_cbt_copy_button_asset_src("list_button_down_leftr.png", btn_left_down, sizeof(btn_left_down));
    kbo_cbt_copy_button_asset_src("list_button_down_mid.png", btn_mid_down, sizeof(btn_mid_down));
    kbo_cbt_copy_button_asset_src("list_button_down_rightr.png", btn_right_down, sizeof(btn_right_down));

    kbo_window_text_appendf(buffer,
        "<style>"
        ".cbtExceptionView{display:grid;grid-template-rows:auto auto minmax(0,1fr);gap:4px;height:100%%;min-height:0}"
        ".cbtExceptionStatus{height:27px;display:flex;align-items:center;gap:14px;padding:0 7px;background:#202020;border:1px solid #171717;border-radius:2px;overflow:hidden}"
        ".cbtProtectPanel{min-width:0;display:flex;align-items:baseline;gap:6px;overflow:hidden}"
        ".cbtProtectPanel:first-child{flex:1 1 auto}.cbtProtectPanel:nth-child(2){flex:0 0 auto}"
        ".cbtProtectLabel{color:#9c9c9c;font-size:12px;font-weight:900;text-transform:uppercase;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".cbtProtectName{color:#f1f1f1;font-size:12px;font-weight:900;line-height:18px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".cbtProtectMeta{color:#b6b6b6;font-size:12px;font-weight:700;line-height:18px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".cbtProtectCredit{color:#ffb13b;font-weight:900}.cbtWindowState{color:#d6a44b;font-weight:900}.cbtWindowState.closed{color:#b6b6b6}"
        ".cbtRulesAsset{height:29px;min-width:86px;display:inline-flex;align-items:center;justify-content:center;padding:0 13px;color:#f0f0f0;font-size:13px;font-weight:900;line-height:29px;background-image:url('%s'),url('%s'),url('%s');background-repeat:no-repeat,repeat-x,no-repeat;background-position:left top,left top,right top;background-size:auto 29px,auto 29px,auto 29px;text-shadow:0 1px 1px #000}"
        ".cbtRulesAsset:hover{background-image:url('%s'),url('%s'),url('%s')}.cbtRulesAsset:active{background-image:url('%s'),url('%s'),url('%s')}"
        ".cbtExceptionTable .roAction{width:46px;text-align:center}.cbtExceptionTable .roRank{width:52px;text-align:right}.cbtExceptionTable .roName{width:260px}.cbtExceptionTable .roSalary{width:96px;text-align:right}.cbtExceptionTable .roCredit{width:96px;text-align:right}.cbtExceptionTable .roYears{width:96px;text-align:right}.cbtExceptionTable .roStatus{width:112px}.cbtExceptionTable tr.selected td{color:#d6a44b!important}"
        ".cbtCurrentWrap{flex:0 0 auto;max-height:86px}.cbtCurrentTable .roLabel{width:164px;color:#9c9c9c;font-weight:900;text-transform:uppercase}.cbtCurrentTable .roName{font-weight:900;color:#f1f1f1}.cbtCurrentTable .roStatus{width:120px;color:#d6a44b;font-weight:900}.cbtCurrentTable .roAction{width:46px;text-align:center}"
        ".cbtIconAction{display:inline-flex;align-items:center;justify-content:center;width:24px;height:24px;border:0;border-radius:0;background-color:transparent;background-repeat:no-repeat;background-position:center center;background-size:24px 24px;color:transparent;font-size:0;line-height:0;text-decoration:none;cursor:pointer}"
        ".cbtProtectAction{background-image:url('%s')}.cbtProtectAction:hover{background-image:url('%s')}.cbtProtectAction:active{background-image:url('%s')}"
        ".cbtClearAction{background-image:url('%s')}.cbtClearAction:hover{background-image:url('%s')}.cbtClearAction:active{background-image:url('%s')}"
        ".cbtActionMuted{display:inline-flex;align-items:center;justify-content:center;width:24px;height:24px;color:#8c8c8c;font-size:12px;font-weight:900}"
        "</style>",
        btn_left, btn_mid, btn_right,
        btn_left_over, btn_mid_over, btn_right_over,
        btn_left_down, btn_mid_down, btn_right_down,
        plus_up, plus_over, plus_down,
        minus_up, minus_over, minus_down);
}

void kbo_webview_append_cbt_exceptions_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    uint32_t year = 0, month = 0, day = 0;
    kbo_current_date_is_valid(&year, &month, &day);
    uint32_t current_date = year * 10000u + month * 100u + day;

    char team_name[96];
    kbo_hub_copy_team_display_name_by_id(selected_team_id, team_name, sizeof(team_name), "No team");

    KboFaSalarySnapshotGrade* grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    KboCbtExceptionDesignation designations[KBO_CBT_EXCEPTION_MAX];
    int designation_count = kbo_cbt_exception_load_designations(designations, KBO_CBT_EXCEPTION_MAX);
    int grade_count = grades != NULL && year > 0
        ? kbo_fa_salary_snapshot_load_grade_rows(year, grades, KBO_FA_SALARY_SNAPSHOT_GRADE_MAX, NULL, 0)
        : 0;

    uint32_t opening_day = 0u;
    kbo_cbt_exception_resolve_opening_day(year, &opening_day);
    int window_open = kbo_cbt_exception_designation_window_open(year, current_date);

    const KboCbtExceptionDesignation* current = NULL;
    for (int i = 0; i < designation_count; i++) {
        if (designations[i].season == year && designations[i].team_id == selected_team_id) {
            current = &designations[i];
            break;
        }
    }

    char summary[256];
    snprintf(summary, sizeof(summary), "Exception player - %s", team_name);
    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary);
    kbo_webview_append_cbt_exception_css(buffer);
    kbo_window_text_appendf(buffer,
        "<div class='cbtExceptionView'><section class='cbtExceptionStatus'>");
    if (current != NULL) {
        int season_count = 0;
        char salary_text[32] = "-";
        char credit_text[32] = "-";
        kbo_cbt_exception_player_eligible(selected_team_id, current->player_key, &season_count);
        for (int i = 0; i < grade_count; i++) {
            if (grades[i].ranking_team_id == selected_team_id
                    && strcmp(grades[i].player_key, current->player_key) == 0) {
                kbo_cbt_format_usd(salary_text, sizeof(salary_text), grades[i].salary);
                kbo_cbt_format_usd(credit_text, sizeof(credit_text), grades[i].salary / 2);
                break;
            }
        }
        kbo_window_text_appendf(buffer,
            "<div class='cbtProtectPanel'><div class='cbtProtectLabel'>Protected Exception Player</div><div class='cbtProtectName'>");
        if (current->player_name[0] != '\0') {
            kbo_html_append_escaped(buffer, current->player_name);
        } else {
            kbo_html_append_escaped(buffer, current->player_key);
        }
        kbo_window_text_appendf(buffer, "</div><div class='cbtProtectMeta'>");
        kbo_html_append_escaped(buffer, salary_text);
        kbo_window_text_appendf(buffer,
            " salary - <span class='cbtProtectCredit'>");
        kbo_html_append_escaped(buffer, credit_text);
        kbo_window_text_appendf(buffer,
            "</span> CBT credit - %d team seasons</div></div>",
            season_count);
    } else {
        kbo_window_text_appendf(buffer,
            "<div class='cbtProtectPanel'><div class='cbtProtectLabel'>Protected Exception Player</div>"
            "<div class='cbtProtectName'>None</div><div class='cbtProtectMeta'>Choose one eligible player from the list below.</div></div>");
    }
    kbo_window_text_appendf(
        buffer,
        "<div class='cbtProtectPanel'><div class='cbtProtectLabel'>Designation Window</div>"
        "<div class='cbtProtectName'><span class='cbtWindowState %s'>%s</span></div>"
        "<div class='cbtProtectMeta'>Opening day %u - deadline %u</div></div>"
        "<a class='cbtRulesAsset' href='kbo://cbt/%d'>Rules</a></section>",
        window_open ? "" : "closed",
        window_open ? "Open" : "Closed",
        opening_day,
        opening_day != 0u ? kbo_add_days_yyyymmdd(opening_day, 6u) : 0u,
        KBO_HUB_CBT_SUBVIEW_RULES);

    kbo_window_text_appendf(buffer,
        "<section class='tablewrap rosterTableWrap cbtCurrentWrap'><table class='ootpRosterTable cbtCurrentTable'><tbody>");
    if (current != NULL) {
        int season_count = 0;
        char salary_text[32] = "-";
        char credit_text[32] = "-";
        kbo_cbt_exception_player_eligible(selected_team_id, current->player_key, &season_count);
        for (int i = 0; i < grade_count; i++) {
            if (grades[i].ranking_team_id == selected_team_id
                    && strcmp(grades[i].player_key, current->player_key) == 0) {
                kbo_cbt_format_usd(salary_text, sizeof(salary_text), grades[i].salary);
                kbo_cbt_format_usd(credit_text, sizeof(credit_text), grades[i].salary / 2);
                break;
            }
        }
        kbo_window_text_appendf(buffer,
            "<tr class='selected'><td class='roLabel'>Current Protected</td><td class='roName'>");
        kbo_html_append_escaped(buffer, current->player_name[0] != '\0' ? current->player_name : current->player_key);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, salary_text);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, credit_text);
        kbo_window_text_appendf(buffer, "</td><td>%d seasons</td><td class='roStatus'>Protected</td><td class='roAction'>", season_count);
        if (window_open) {
            kbo_window_text_appendf(
                buffer,
                "<a class='cbtIconAction cbtClearAction' title='Clear exception player' href='kbo://cbt_exception/clear/%u/%u'>Clear</a>",
                year,
                selected_team_id);
        } else {
            kbo_window_text_appendf(buffer, "<span class='cbtActionMuted'>-</span>");
        }
        kbo_window_text_appendf(buffer, "</td></tr>");
    } else {
        const char* none_status = window_open
            ? "No exception player designated for this team."
            : "No designation was recorded before the deadline.";
        kbo_window_text_appendf(buffer,
            "<tr><td class='roLabel'>Current Protected</td><td class='roName'>None</td>"
            "<td colspan='5' class='roStatus'>");
        kbo_html_append_escaped(buffer, none_status);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");

    kbo_window_text_appendf(buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable cbtExceptionTable'><thead><tr>"
        "<th data-sort-type='text' class='roAction'>Set</th>"
        "<th data-sort-type='number' class='roRank'>Rank</th>"
        "<th data-sort-type='text' class='roName'>Player</th>"
        "<th data-sort-type='number' class='roSalary'>Salary</th>"
        "<th data-sort-type='number' class='roCredit'>CBT Credit</th>"
        "<th data-sort-type='number' class='roYears'>Seasons</th>"
        "<th data-sort-type='text' class='roStatus'>Status</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    for (int i = 0; i < grade_count; i++) {
        const KboFaSalarySnapshotGrade* grade = &grades[i];
        if (grade->ranking_team_id != selected_team_id
                || grade->foreign_flag != 0u
                || grade->salary <= 0
                || grade->player_key[0] == '\0') {
            continue;
        }
        int season_count = 0;
        if (!kbo_cbt_exception_player_eligible(selected_team_id, grade->player_key, &season_count)) {
            continue;
        }
        char salary[32];
        char credit[32];
        kbo_cbt_format_usd(salary, sizeof(salary), grade->salary);
        kbo_cbt_format_usd(credit, sizeof(credit), grade->salary / 2);
        int designated = current != NULL && strcmp(current->player_key, grade->player_key) == 0;
        kbo_window_text_appendf(
            buffer,
            "<tr%s><td class='roAction'>",
            designated ? " class='selected'" : "");
        if (window_open) {
            if (designated) {
                kbo_window_text_appendf(
                    buffer,
                    "<a class='cbtIconAction cbtClearAction' title='Clear exception player' href='kbo://cbt_exception/clear/%u/%u'>Clear</a>",
                    year,
                    selected_team_id);
            } else {
                kbo_window_text_appendf(
                    buffer,
                    "<a class='cbtIconAction cbtProtectAction' title='Protect as exception player' href='kbo://cbt_exception/set/%u/%u/",
                    year,
                    selected_team_id);
                kbo_html_append_escaped(buffer, grade->player_key);
                kbo_window_text_appendf(buffer, "'>Set</a>");
            }
        } else {
            kbo_window_text_appendf(buffer, "<span class='cbtActionMuted'>-</span>");
        }
        kbo_window_text_appendf(buffer, "</td><td class='roRank'>%u</td><td class='roName'>", grade->team_rank);
        kbo_html_append_escaped(buffer, grade->player_name[0] != '\0' ? grade->player_name : grade->player_key);
        kbo_window_text_appendf(buffer, "</td><td class='roSalary'>");
        kbo_html_append_escaped(buffer, salary);
        kbo_window_text_appendf(buffer, "</td><td class='roCredit'>");
        kbo_html_append_escaped(buffer, credit);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roYears'>%d</td><td class='roStatus'><strong>%s</strong></td></tr>",
            season_count,
            designated ? "PROTECTED" : "Eligible");
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer,
            "<tr><td colspan='7' class='roEmptyMessage'>No eligible exception candidates found for the selected team snapshot.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div></div>");
    if (grades != NULL) HeapFree(GetProcessHeap(), 0, grades);
}

void kbo_webview_append_cbt_rules_view(KboWindowTextBuffer* buffer)
{
    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    uint32_t current_year = 0, cm = 0, cd = 0;
    kbo_current_date_is_valid(&current_year, &cm, &cd);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, rules.enabled ? "CBT Enabled" : "CBT Disabled");
    kbo_window_text_appendf(buffer,
        "<section class='tablewrap rosterTableWrap' style='margin-bottom:12px'>"
        "<table class='ootpRosterTable'><thead><tr>"
        "<th data-sort-type='text' style='width:240px'>Setting</th><th data-sort-type='text'>Value</th>"
        "</tr></thead><tbody>"
        "<tr><td>Top Player Count</td><td>%u</td></tr>"
        "<tr><td>Annual Cap Increase</td><td>%u%%</td></tr>"
        "<tr><td>Tax Rate - 1st violation</td><td>%u%%</td></tr>"
        "<tr><td>Tax Rate - 2nd violation</td><td>%u%%</td></tr>"
        "<tr><td>Tax Rate - 3rd+ violation</td><td>%u%%</td></tr>"
        "<tr><td>Draft Penalty (min consecutive)</td><td>%u</td></tr>"
        "<tr><td>Draft Penalty (stages)</td><td>%u</td></tr>"
        "</tbody></table></section>",
        rules.top_player_count, rules.annual_increase_pct,
        rules.tax_rate_1, rules.tax_rate_2, rules.tax_rate_3plus,
        rules.draft_penalty_min_consecutive, rules.draft_penalty_stages);

    if (current_year > 0) {
        kbo_window_text_appendf(buffer,
            "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable'><thead><tr>"
            "<th data-sort-type='number' style='width:88px'>Season</th>"
            "<th data-sort-type='number' style='width:120px'>Cap Threshold</th>"
            "</tr></thead><tbody>");
        uint32_t start = current_year >= 2 ? current_year - 2 : current_year;
        for (uint32_t y = start; y <= current_year + 2; y++) {
            char text[32];
            const char* row_style = (y == current_year) ? " style='color:#d6a44b'" : "";
            kbo_cbt_format_usd(text, sizeof(text), kbo_cbt_get_threshold(&rules, y));
            kbo_window_text_appendf(buffer, "<tr%s><td>%u</td><td>", row_style, y);
            kbo_html_append_escaped(buffer, text);
            kbo_window_text_appendf(buffer, y >= current_year + 1 ? " (proj.)</td></tr>" : "</td></tr>");
        }
        kbo_window_text_appendf(buffer, "</tbody></table></section>");
    }

    kbo_window_text_appendf(buffer, "</div>");
}
