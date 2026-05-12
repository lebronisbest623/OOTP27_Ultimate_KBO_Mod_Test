#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../../competitive_balance_tax/records/cbt_records.h"
#include "../../../competitive_balance_tax/rules/cbt_rules.h"
#include "../../../core/dates/core_current_date.h"
#include "../../support/assets/names/support_names.h"
#include "../../support/roster/cells/ui_roster_cells.h"
#include "../../support/text/buffer/ui_text_buffer.h"
#include "ui_cbt_view.h"

static int kbo_cbt_view_compare(const void* a, const void* b)
{
    const KboCbtRecord* ra = (const KboCbtRecord*)a;
    const KboCbtRecord* rb = (const KboCbtRecord*)b;
    if (ra->season != rb->season) {
        return ra->season > rb->season ? -1 : 1;
    }
    if (ra->overage != rb->overage) {
        return ra->overage > rb->overage ? -1 : 1;
    }
    return 0;
}

static void kbo_cbt_format_usd(char* out, size_t out_size, int32_t value)
{
    if (value <= 0) {
        snprintf(out, out_size, "-");
        return;
    }
    if (value >= 1000000) {
        int m = value / 1000000;
        int tenth = (value % 1000000) / 100000;
        if (tenth == 0) {
            snprintf(out, out_size, "$%dM", m);
        } else {
            snprintf(out, out_size, "$%d.%dM", m, tenth);
        }
    } else if (value >= 1000) {
        snprintf(out, out_size, "$%dk", value / 1000);
    } else {
        snprintf(out, out_size, "$%d", value);
    }
}

static void kbo_cbt_append_team_summary(
    KboWindowTextBuffer* buffer,
    const KboCbtRecord* records,
    int count,
    const KboCbtRules* rules,
    const char* is_latest)
{
    /* Find up to 3 most recent unique seasons (records already sorted season desc). */
    uint32_t seasons[3] = {0, 0, 0};
    int season_count = 0;
    for (int i = 0; i < count && season_count < 3; i++) {
        uint32_t s = records[i].season;
        int found = 0;
        for (int j = 0; j < season_count; j++) {
            if (seasons[j] == s) { found = 1; break; }
        }
        if (!found) seasons[season_count++] = s;
    }
    if (season_count == 0) return;

    /* Collect unique teams in order of first appearance (most recent season first). */
    uint32_t team_ids[64];
    int team_count = 0;
    for (int i = 0; i < count && team_count < 64; i++) {
        uint32_t tid = records[i].team_id;
        int found = 0;
        for (int j = 0; j < team_count; j++) {
            if (team_ids[j] == tid) { found = 1; break; }
        }
        if (!found) team_ids[team_count++] = tid;
    }
    if (team_count == 0) return;

    kbo_window_text_appendf(buffer,
        "<section class='tablewrap rosterTableWrap' style='margin-bottom:12px'>"
        "<table class='ootpRosterTable'><thead><tr>"
        "<th data-sort-type='text'>Team</th>");
    for (int s = 0; s < season_count; s++) {
        kbo_window_text_appendf(buffer,
            "<th data-sort-type='number' style='width:120px'>%u</th>", seasons[s]);
    }
    kbo_window_text_appendf(buffer, "<th data-sort-type='number' style='width:76px'>Draft Pen.</th></tr></thead><tbody>");

    for (int t = 0; t < team_count; t++) {
        uint32_t tid = team_ids[t];
        char team_name[96] = {0};
        /* Grab team name from the most recent record for this team. */
        for (int i = 0; i < count; i++) {
            if (records[i].team_id == tid) {
                if (records[i].team_name[0] != '\0') {
                    snprintf(team_name, sizeof(team_name), "%s", records[i].team_name);
                } else {
                    kbo_hub_copy_team_display_name_by_id(tid, team_name, sizeof(team_name), NULL);
                }
                break;
            }
        }
        if (team_name[0] == '\0') snprintf(team_name, sizeof(team_name), "Team #%u", tid);

        /* Compute draft penalty for this team (most recent record). */
        int draft_stages = 0;
        for (int i = 0; i < count; i++) {
            if (records[i].team_id == tid && is_latest && is_latest[i]) {
                if (records[i].overage > 0
                    && records[i].consecutive_count >= rules->draft_penalty_min_consecutive)
                {
                    draft_stages = (int)rules->draft_penalty_stages;
                }
                break;
            }
        }

        kbo_window_text_appendf(buffer, "<tr><td>");
        kbo_html_append_escaped(buffer, team_name);
        kbo_window_text_appendf(buffer, "</td>");

        for (int s = 0; s < season_count; s++) {
            uint32_t season = seasons[s];
            const KboCbtRecord* rec = NULL;
            for (int i = 0; i < count; i++) {
                if (records[i].team_id == tid && records[i].season == season) {
                    rec = &records[i];
                    break;
                }
            }
            if (rec == NULL) {
                kbo_window_text_appendf(buffer, "<td style='color:#666'>-</td>");
                continue;
            }
            char payroll_text[32] = {0};
            char threshold_text[32] = {0};
            kbo_cbt_format_usd(payroll_text, sizeof(payroll_text), rec->payroll);
            kbo_cbt_format_usd(threshold_text, sizeof(threshold_text), rec->threshold);
            if (rec->overage > 0) {
                kbo_window_text_appendf(buffer, "<td style='color:#e88'>");
                kbo_html_append_escaped(buffer, payroll_text);
                kbo_window_text_appendf(buffer, " / ");
                kbo_html_append_escaped(buffer, threshold_text);
                kbo_window_text_appendf(buffer, "</td>");
            } else {
                kbo_window_text_appendf(buffer, "<td>");
                kbo_html_append_escaped(buffer, payroll_text);
                kbo_window_text_appendf(buffer, " / ");
                kbo_html_append_escaped(buffer, threshold_text);
                kbo_window_text_appendf(buffer, "</td>");
            }
        }

        if (draft_stages > 0) {
            kbo_window_text_appendf(buffer,
                "<td style='color:#e88'>-%d</td></tr>", draft_stages);
        } else {
            kbo_window_text_appendf(buffer, "<td>-</td></tr>");
        }
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section>");
}

static void kbo_webview_append_cbt_records_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    KboCbtRecord* records = (KboCbtRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_RECORDS_MAX * sizeof(KboCbtRecord));
    if (records == NULL) {
        return;
    }

    int count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");

    if (count > 0) {
        qsort(records, (size_t)count, sizeof(KboCbtRecord), kbo_cbt_view_compare);
    }

    char* is_latest = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)(count > 0 ? count : 1));
    if (is_latest) {
        for (int i = 0; i < count; i++) {
            int seen = 0;
            for (int j = 0; j < i; j++) {
                if (records[j].team_id == records[i].team_id) { seen = 1; break; }
            }
            is_latest[i] = seen ? 0 : 1;
        }
    }

    int violations = 0;
    for (int i = 0; i < count; i++) {
        if (records[i].overage > 0) violations++;
    }

    char summary[256] = {0};
    if (count <= 0) {
        snprintf(summary, sizeof(summary), "No records yet — snapshot taken at opening day");
    } else {
        snprintf(summary, sizeof(summary),
            "%d records — %d violation%s",
            count, violations, violations == 1 ? "" : "s");
    }
    kbo_webview_append_roster_top_bar(buffer, summary);

    kbo_cbt_append_team_summary(buffer, records, count, &rules, is_latest);

    kbo_window_text_appendf(buffer,
        "<section class='tablewrap rosterTableWrap'>"
        "<table class='ootpRosterTable'>"
        "<thead><tr>"
        "<th data-sort-type='number' style='width:64px'>Season</th>"
        "<th data-sort-type='text'>Team</th>"
        "<th data-sort-type='number' style='width:88px'>Payroll</th>"
        "<th data-sort-type='number' style='width:88px'>Threshold</th>"
        "<th data-sort-type='number' style='width:88px'>Overage</th>"
        "<th data-sort-type='number' style='width:52px'>Rate</th>"
        "<th data-sort-type='number' style='width:88px'>Tax Paid</th>"
        "<th data-sort-type='number' style='width:60px'>Consec.</th>"
        "<th data-sort-type='text' style='width:80px'>Status</th>"
        "<th data-sort-type='number' style='width:76px'>Draft Pen.</th>"
        "</tr></thead><tbody>");

    if (count <= 0) {
        kbo_window_text_appendf(buffer,
            "<tr><td colspan='10' style='color:#888;text-align:center'>"
            "Records are written when the opening-day salary snapshot is taken."
            "</td></tr>");
    }

    for (int i = 0; i < count; i++) {
        const KboCbtRecord* r = &records[i];
        char team_name[96] = {0};
        if (r->team_name[0] != '\0') {
            snprintf(team_name, sizeof(team_name), "%s", r->team_name);
        } else {
            kbo_hub_copy_team_display_name_by_id(r->team_id, team_name, sizeof(team_name), NULL);
        }
        if (team_name[0] == '\0') {
            snprintf(team_name, sizeof(team_name), "Team #%u", r->team_id);
        }

        char payroll_text[32] = {0};
        char threshold_text[32] = {0};
        char overage_text[32] = {0};
        char tax_text[32] = {0};
        kbo_cbt_format_usd(payroll_text, sizeof(payroll_text), r->payroll);
        kbo_cbt_format_usd(threshold_text, sizeof(threshold_text), r->threshold);
        kbo_cbt_format_usd(overage_text, sizeof(overage_text), r->overage);
        kbo_cbt_format_usd(tax_text, sizeof(tax_text), r->tax_amount);

        const char* status = r->overage > 0 ? "VIOLATION" : "Clean";
        const char* row_style = r->overage > 0
            ? " style='color:#e88;'"
            : "";

        kbo_window_text_appendf(buffer,
            "<tr%s>"
            "<td>%u</td><td>",
            row_style, r->season);
        kbo_html_append_escaped(buffer, team_name);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, payroll_text);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, threshold_text);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, overage_text);
        kbo_window_text_appendf(buffer,
            "</td><td>%u%%</td><td>",
            r->tax_rate_pct);
        kbo_html_append_escaped(buffer, tax_text);
        int draft_stages = 0;
        if (is_latest && is_latest[i]
            && r->overage > 0
            && r->consecutive_count >= rules.draft_penalty_min_consecutive)
        {
            draft_stages = (int)rules.draft_penalty_stages;
        }
        if (draft_stages > 0) {
            kbo_window_text_appendf(buffer,
                "</td><td>%u</td><td>%s</td>"
                "<td style='color:#e88;'>-%d</td></tr>",
                r->consecutive_count, status, draft_stages);
        } else {
            kbo_window_text_appendf(buffer,
                "</td><td>%u</td><td>%s</td><td>-</td></tr>",
                r->consecutive_count, status);
        }
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    if (is_latest) HeapFree(GetProcessHeap(), 0, is_latest);
    HeapFree(GetProcessHeap(), 0, records);
}

static void kbo_webview_append_cbt_rules_view(KboWindowTextBuffer* buffer)
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
        "<th data-sort-type='text' style='width:240px'>Setting</th>"
        "<th data-sort-type='text'>Value</th>"
        "</tr></thead><tbody>");

    kbo_window_text_appendf(buffer,
        "<tr><td>Top Player Count</td><td>%u</td></tr>"
        "<tr><td>Annual Cap Increase</td><td>%u%%</td></tr>"
        "<tr><td>Tax Rate — 1st violation</td><td>%u%%</td></tr>"
        "<tr><td>Tax Rate — 2nd violation</td><td>%u%%</td></tr>"
        "<tr><td>Tax Rate — 3rd+ violation</td><td>%u%%</td></tr>"
        "<tr><td>Draft Penalty (min consecutive)</td><td>%u</td></tr>"
        "<tr><td>Draft Penalty (stages)</td><td>%u</td></tr>",
        rules.top_player_count,
        rules.annual_increase_pct,
        rules.tax_rate_1, rules.tax_rate_2, rules.tax_rate_3plus,
        rules.draft_penalty_min_consecutive, rules.draft_penalty_stages);

    kbo_window_text_appendf(buffer, "</tbody></table></section>");

    if (current_year > 0) {
        kbo_window_text_appendf(buffer,
            "<section class='tablewrap rosterTableWrap'>"
            "<table class='ootpRosterTable'><thead><tr>"
            "<th data-sort-type='number' style='width:88px'>Season</th>"
            "<th data-sort-type='number' style='width:120px'>Cap Threshold</th>"
            "</tr></thead><tbody>");

        uint32_t start = current_year >= 2 ? current_year - 2 : current_year;
        for (uint32_t y = start; y <= current_year + 2; y++) {
            int32_t threshold = kbo_cbt_get_threshold(&rules, y);
            char threshold_text[32] = {0};
            kbo_cbt_format_usd(threshold_text, sizeof(threshold_text), threshold);
            const char* row_style = (y == current_year) ? " style='color:#d6a44b'" : "";
            kbo_window_text_appendf(buffer, "<tr%s><td>%u</td><td>", row_style, y);
            kbo_html_append_escaped(buffer, threshold_text);
            if (y >= current_year + 1) {
                kbo_window_text_appendf(buffer, " (proj.)</td></tr>");
            } else {
                kbo_window_text_appendf(buffer, "</td></tr>");
            }
        }

        kbo_window_text_appendf(buffer, "</tbody></table></section>");
    }

    kbo_window_text_appendf(buffer, "</div>");
}

void kbo_webview_append_cbt_view(KboWindowTextBuffer* buffer, int subview)
{
    if (buffer == NULL) return;
    if (subview == KBO_HUB_CBT_SUBVIEW_RULES) {
        kbo_webview_append_cbt_rules_view(buffer);
    } else {
        kbo_webview_append_cbt_records_view(buffer);
    }
}
