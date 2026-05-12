#include "ui_cbt_view_internal.h"

static int kbo_cbt_view_compare(const void* a, const void* b)
{
    const KboCbtRecord* ra = (const KboCbtRecord*)a;
    const KboCbtRecord* rb = (const KboCbtRecord*)b;
    if (ra->season != rb->season) return ra->season > rb->season ? -1 : 1;
    if (ra->overage != rb->overage) return ra->overage > rb->overage ? -1 : 1;
    return 0;
}

void kbo_cbt_format_usd(char* out, size_t out_size, int32_t value)
{
    if (value <= 0) {
        snprintf(out, out_size, "-");
    } else if (value >= 1000000) {
        int m = value / 1000000;
        int tenth = (value % 1000000) / 100000;
        snprintf(out, out_size, tenth == 0 ? "$%dM" : "$%d.%dM", m, tenth);
    } else if (value >= 1000) {
        snprintf(out, out_size, "$%dk", value / 1000);
    } else {
        snprintf(out, out_size, "$%d", value);
    }
}

int kbo_cbt_load_sorted(KboCbtRecord** out_records, int* out_count)
{
    KboCbtRecord* records = (KboCbtRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_RECORDS_MAX * sizeof(KboCbtRecord));
    if (records == NULL) return 0;
    *out_count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);
    if (*out_count > 0) {
        qsort(records, (size_t)*out_count, sizeof(KboCbtRecord), kbo_cbt_view_compare);
    }
    *out_records = records;
    return 1;
}

int kbo_cbt_is_latest_for_team(const KboCbtRecord* records, int index)
{
    for (int i = 0; i < index; i++) {
        if (records[i].team_id == records[index].team_id) return 0;
    }
    return 1;
}

void kbo_cbt_team_name(const KboCbtRecord* rec, char* out, size_t out_size)
{
    out[0] = '\0';
    if (rec->team_name[0] != '\0') {
        snprintf(out, out_size, "%s", rec->team_name);
    } else {
        kbo_hub_copy_team_display_name_by_id(rec->team_id, out, out_size, NULL);
    }
    if (out[0] == '\0') snprintf(out, out_size, "Team #%u", rec->team_id);
}

static int kbo_cbt_ratio_pct(const KboCbtRecord* rec)
{
    if (rec->threshold <= 0) return 0;
    return (int)((int64_t)rec->payroll * 100LL / (int64_t)rec->threshold);
}

static void kbo_cbt_format_ratio_label(char* out, size_t out_size, const KboCbtRecord* rec)
{
    int ratio = rec->threshold > 0
        ? (int)((int64_t)rec->payroll * 1000LL / (int64_t)rec->threshold)
        : 0;
    if (ratio >= 1000) {
        snprintf(out, out_size, "%d.%03d", ratio / 1000, ratio % 1000);
    } else {
        snprintf(out, out_size, ".%03d", ratio);
    }
}

static void kbo_cbt_append_overview_css(KboWindowTextBuffer* buffer)
{
    kbo_window_text_appendf(buffer,
        "<style>"
        ".cbtOverview{display:grid;grid-template-rows:auto minmax(0,1fr);gap:4px;height:100%%;min-height:0}"
        ".cbtMetrics{height:25px;display:flex;align-items:center;gap:14px;padding:0 7px;background:#202020;border:1px solid #171717;border-radius:2px;overflow:hidden}"
        ".cbtMetric{min-width:0;display:flex;align-items:baseline;gap:5px;overflow:hidden}"
        ".cbtMetricLabel{color:#9c9c9c;font-size:12px;font-weight:900;text-transform:uppercase;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".cbtMetricValue{color:#f0f0f0;font-size:12px;font-weight:900;line-height:18px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".cbtMetricValue.warn{color:#d2d2d2}.cbtMetricValue.good{color:#d2d2d2}"
        ".cbtChartWrap{min-height:0;padding:7px 9px 9px;overflow:hidden}"
        ".cbtChartTitle{height:20px;color:#bebebe;font-size:15px;font-weight:900;line-height:18px;text-transform:uppercase}"
        ".cbtPlot{position:relative;height:calc(100%% - 22px);min-height:230px;padding:0 0 24px 48px;background:#1c1c1c;border-left:1px solid #080808;border-bottom:1px solid #080808;overflow:hidden}"
        ".cbtGrid{position:absolute;left:48px;right:0;height:1px;background:#383838}.cbtGrid.zero{background:#080808}.cbtAxisLabel{position:absolute;left:0;width:41px;margin-top:-7px;color:#e1e1e1;font-size:12px;font-weight:900;text-align:right}"
        ".cbtBars{position:absolute;left:48px;right:0;bottom:24px;top:0;display:flex;align-items:flex-end;gap:5px;padding:0 2px}"
        ".cbtBarSlot{position:relative;flex:1 1 0;min-width:28px;height:100%%;display:flex;align-items:flex-end;justify-content:center}"
        ".cbtBar{width:100%%;max-width:54px;background:#a00000}.cbtBar.clean{background:#008f00}.cbtBarLabel{position:absolute;left:50%%;transform:translateX(-50%%);margin-top:-16px;color:#f0f0f0;font-size:12px;font-weight:900;text-shadow:0 1px 1px #000;white-space:nowrap}"
        ".cbtBarTeam{position:absolute;left:50%%;bottom:-20px;transform:translateX(-50%%);max-width:64px;color:#f0f0f0;font-size:12px;font-weight:900;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;text-align:center}"
        "</style>");
}

static void kbo_webview_append_cbt_overview_view(KboWindowTextBuffer* buffer)
{
    KboCbtRecord* records = NULL;
    int count = 0;
    if (!kbo_cbt_load_sorted(&records, &count)) return;

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);

    int clubs = 0, violations = 0, penalties = 0;
    int32_t total_tax = 0;
    uint32_t latest_season = count > 0 ? records[0].season : 0u;
    int32_t latest_threshold = count > 0 ? records[0].threshold : 0;
    for (int i = 0; i < count; i++) {
        if (!kbo_cbt_is_latest_for_team(records, i)) continue;
        clubs++;
        if (records[i].overage > 0) {
            violations++;
            total_tax += records[i].tax_amount;
            if (records[i].consecutive_count >= rules.draft_penalty_min_consecutive) penalties++;
        }
    }

    char summary[256];
    snprintf(summary, sizeof(summary),
        count > 0 ? "%u season overview - %d clubs - %d over tax line" : "No records yet - snapshot taken at opening day",
        latest_season, clubs, violations);

    char tax_text[32], threshold_text[32];
    kbo_cbt_format_usd(tax_text, sizeof(tax_text), total_tax);
    kbo_cbt_format_usd(threshold_text, sizeof(threshold_text), latest_threshold);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary);
    kbo_cbt_append_overview_css(buffer);
    kbo_window_text_appendf(buffer,
        "<div class='cbtOverview'><section class='cbtMetrics'>"
        "<div class='cbtMetric'><span class='cbtMetricLabel'>Current Threshold</span><span class='cbtMetricValue'>");
    kbo_html_append_escaped(buffer, threshold_text);
    kbo_window_text_appendf(buffer,
        "</span></div><div class='cbtMetric'><span class='cbtMetricLabel'>Clubs Over Line</span><span class='cbtMetricValue %s'>%d</span></div>"
        "<div class='cbtMetric'><span class='cbtMetricLabel'>Total Tax Due</span><span class='cbtMetricValue %s'>",
        violations > 0 ? "warn" : "good", violations, total_tax > 0 ? "warn" : "good");
    kbo_html_append_escaped(buffer, tax_text);
    kbo_window_text_appendf(buffer,
        "</span></div><div class='cbtMetric'><span class='cbtMetricLabel'>Draft Penalties</span><span class='cbtMetricValue %s'>%d</span></div>"
        "</section><section class='tablewrap rosterTableWrap cbtChartWrap'>"
        "<div class='cbtChartTitle'>League Tax Line</div>"
        "<div class='cbtPlot'>"
        "<div class='cbtGrid' style='top:0%%'></div><div class='cbtAxisLabel' style='top:0%%'>1.400</div>"
        "<div class='cbtGrid' style='top:14.2857%%'></div><div class='cbtAxisLabel' style='top:14.2857%%'>1.200</div>"
        "<div class='cbtGrid' style='top:28.5714%%'></div><div class='cbtAxisLabel' style='top:28.5714%%'>1.000</div>"
        "<div class='cbtGrid' style='top:42.8571%%'></div><div class='cbtAxisLabel' style='top:42.8571%%'>.800</div>"
        "<div class='cbtGrid' style='top:57.1428%%'></div><div class='cbtAxisLabel' style='top:57.1428%%'>.600</div>"
        "<div class='cbtGrid' style='top:71.4285%%'></div><div class='cbtAxisLabel' style='top:71.4285%%'>.400</div>"
        "<div class='cbtGrid' style='top:85.7142%%'></div><div class='cbtAxisLabel' style='top:85.7142%%'>.200</div>"
        "<div class='cbtGrid zero' style='top:100%%'></div><div class='cbtAxisLabel' style='top:100%%'>.000</div>"
        "<div class='cbtBars'>",
        penalties > 0 ? "warn" : "good", penalties);

    if (count <= 0) {
        kbo_window_text_appendf(buffer,
            "<div style='color:#888;text-align:center;padding:22px'>Records are written when the opening-day salary snapshot is taken.</div>");
    }

    for (int i = 0; i < count; i++) {
        const KboCbtRecord* rec = &records[i];
        if (!kbo_cbt_is_latest_for_team(records, i)) continue;
        char team[96], label[16];
        int ratio = kbo_cbt_ratio_pct(rec);
        int capped = ratio < 0 ? 0 : (ratio > 140 ? 140 : ratio);
        int height = capped * 100 / 140;
        kbo_cbt_team_name(rec, team, sizeof(team));
        kbo_cbt_format_ratio_label(label, sizeof(label), rec);

        kbo_window_text_appendf(buffer,
            "<div class='cbtBarSlot'><div class='cbtBarLabel' style='bottom:%d%%'>",
            height);
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer,
            "</div><div class='cbtBar %s' style='height:%d%%'></div><div class='cbtBarTeam'>",
            rec->overage > 0 ? "over" : "clean",
            height);
        kbo_html_append_escaped(buffer, team);
        kbo_window_text_appendf(buffer, "</div></div>");
    }

    kbo_window_text_appendf(buffer, "</div></div></section></div></div>");
    HeapFree(GetProcessHeap(), 0, records);
}

void kbo_webview_append_cbt_view(KboWindowTextBuffer* buffer, int subview, uint32_t selected_team_id)
{
    if (buffer == NULL) return;
    if (subview == KBO_HUB_CBT_SUBVIEW_RULES) {
        kbo_webview_append_cbt_rules_view(buffer);
    } else if (subview == KBO_HUB_CBT_SUBVIEW_EXCEPTIONS) {
        kbo_webview_append_cbt_exceptions_view(buffer, selected_team_id);
    } else if (subview == KBO_HUB_CBT_SUBVIEW_HISTORY) {
        kbo_webview_append_cbt_history_view(buffer, selected_team_id);
    } else {
        kbo_webview_append_cbt_overview_view(buffer);
    }
}
