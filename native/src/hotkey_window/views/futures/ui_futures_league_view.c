#include "ui_futures_league_view.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../runtime/hotkey_window_runtime_shared.h"
#include "../../../team/independent_acquisition/independent_acquisition_ui.h"

static void kbo_futures_ui_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u/%02u/%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static void kbo_futures_ui_format_cash(int32_t value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (value >= 1000000 || value <= -1000000) {
        snprintf(out, out_size, "$%.1fM", (double)value / 1000000.0);
        return;
    }
    if (value >= 1000 || value <= -1000) {
        snprintf(out, out_size, "$%dk", value / 1000);
        return;
    }
    snprintf(out, out_size, "$%d", value);
}

static void kbo_futures_ui_copy_player_name(uintptr_t player_ptr, uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (kbo_player_pointer_plausible(player_ptr)
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_hub_copy_player_display_name((uint8_t*)player_ptr, out, out_size);
    }
    if (out[0] == '\0') {
        snprintf(out, out_size, "#%u", player_id);
    }
}

static const char* kbo_futures_ui_position_label(uintptr_t player_ptr)
{
    if (kbo_player_pointer_plausible(player_ptr)
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return kbo_webview_player_position_label((uint8_t*)player_ptr, 0u);
    }
    return "-";
}

static void kbo_futures_ui_copy_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    kbo_hub_copy_team_display_name_by_id(team_id, out, out_size, NULL);
    if (out[0] == '\0') {
        snprintf(out, out_size, "#%u", team_id);
    }
}

static uint32_t kbo_futures_ui_read_team_league_id(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

static uint32_t kbo_futures_ui_resolve_buyer_team_id(uint32_t selected_team_id)
{
    if (selected_team_id == 0u) {
        return 0u;
    }

    uint8_t* selected_team = find_kbo_team_by_numeric_id_any_league(selected_team_id, 1);
    if (selected_team == NULL || !memory_range_readable(selected_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    if (kbo_futures_ui_read_team_league_id(selected_team) == kbo_league_id) {
        return selected_team_id;
    }

    if (!memory_range_readable(selected_team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }

    uint32_t parent_team_id = *(uint32_t*)(selected_team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    if (parent_team_id == 0u || parent_team_id == selected_team_id) {
        return 0u;
    }

    uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
    if (parent_team == NULL || !memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    if (kbo_futures_ui_read_team_league_id(parent_team) != kbo_league_id) {
        return 0u;
    }

    return parent_team_id;
}

static void kbo_futures_ui_append_context_bar(
    KboWindowTextBuffer* buffer,
    const KboIndependentAcquisitionUiContext* context,
    int row_count,
    const char* mode)
{
    char open_text[16] = "-";
    char close_text[16] = "-";
    char cash_text[32] = "$0";
    char buyer_name[96] = "-";
    char summary[384] = {0};
    if (context != NULL) {
        kbo_futures_ui_format_yyyymmdd(context->open_date, open_text, sizeof(open_text));
        kbo_futures_ui_format_yyyymmdd(context->close_date, close_text, sizeof(close_text));
        kbo_futures_ui_format_cash(context->buyer_cash, cash_text, sizeof(cash_text));
        if (context->buyer_team_id != 0u) {
            kbo_futures_ui_copy_team_name(context->buyer_team_id, buyer_name, sizeof(buyer_name));
        }
        snprintf(
            summary,
            sizeof(summary),
            "View: %s - Buyer: %s - Window: %s - Period: %s to %s - Cash: %s - Sellers: %d - Rows: %d",
            mode != NULL ? mode : "-",
            buyer_name,
            context->window_open ? "Open" : "Closed",
            open_text,
            close_text,
            cash_text,
            context->seller_count,
            row_count);
    } else {
        snprintf(summary, sizeof(summary), "View: %s - Rows: %d", mode != NULL ? mode : "-", row_count);
    }
    kbo_webview_append_roster_top_bar(buffer, summary);
}

static void kbo_futures_ui_append_empty_row(KboWindowTextBuffer* buffer, int colspan, const char* text)
{
    kbo_window_text_appendf(buffer, "<tr><td class='roEmptyMessage' colspan='%d'>", colspan);
    kbo_html_append_escaped(buffer, text != NULL && text[0] != '\0' ? text : "-");
    kbo_window_text_appendf(buffer, "</td></tr>");
}

static void kbo_futures_ui_append_offer_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    uint32_t buyer_team_id = kbo_futures_ui_resolve_buyer_team_id(selected_team_id);
    KboIndependentAcquisitionUiOfferRow rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS];
    KboIndependentAcquisitionUiContext context;
    int count = kbo_independent_acquisition_ui_collect_offer_rows(
        buyer_team_id,
        rows,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS,
        &context);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights futuresRights'>");
    kbo_futures_ui_append_context_bar(buffer, &context, count, "OFFER");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable futuresOfferTable'><thead><tr>"
        "<th class='roAction'>Offer</th><th class='roPo' data-sort-type='text'>PO</th>"
        "<th class='roName' data-sort-type='text'>Name</th><th class='roClub' data-sort-type='text'>Independent Club</th>"
        "<th class='roNat' data-sort-type='text'>Nation</th><th class='roAge' data-sort-type='number'>Age</th>"
        "<th class='roSlot' data-sort-type='text'>Slot</th><th class='roCash' data-sort-type='number'>Cash</th>"
        "<th class='roScore' data-sort-type='number'>Score</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    if (!context.policy_enabled) {
        kbo_futures_ui_append_empty_row(buffer, 10, "Custom foreign policy is disabled.");
    } else if (!context.buyer_valid) {
        kbo_futures_ui_append_empty_row(buffer, 10, "Select a KBO club first.");
    } else if (context.seller_count <= 0) {
        kbo_futures_ui_append_empty_row(buffer, 10, "No independent Futures League club is resolved from the seed file.");
    } else if (count <= 0) {
        kbo_futures_ui_append_empty_row(buffer, 10, "No eligible independent club player is available for this club.");
    }

    for (int i = 0; i < count; i++) {
        KboIndependentAcquisitionUiOfferRow* row = &rows[i];
        char player_name[96] = {0};
        char seller_name[96] = {0};
        char cash_text[32] = {0};
        char status[32] = "Ready";
        kbo_futures_ui_copy_player_name(row->player_ptr, row->player_id, player_name, sizeof(player_name));
        kbo_futures_ui_copy_team_name(row->seller_team_id, seller_name, sizeof(seller_name));
        kbo_futures_ui_format_cash(row->cash_cost, cash_text, sizeof(cash_text));
        if (row->already_decided) {
            snprintf(status, sizeof(status), "Result");
        } else if (row->already_requested) {
            snprintf(status, sizeof(status), "Pending");
        } else if (!context.window_open) {
            snprintf(status, sizeof(status), "Closed");
        }

        kbo_window_text_appendf(buffer, "<tr><td class='roAction'><span class='rightsActions'>");
        if (context.window_open && !row->already_requested && !row->already_decided) {
            kbo_window_text_appendf(
                buffer,
                "<a class='rightsAction rightsAdd' title='Submit offer' href='kbo://futures-offer/submit/%u/%u/%u'>Offer</a>",
                buyer_team_id,
                row->seller_team_id,
                row->player_id);
        } else {
            kbo_window_text_appendf(buffer, "<span class='rightsAction rightsAdd disabled' title='");
            kbo_html_append_escaped(buffer, status);
            kbo_window_text_appendf(buffer, "'>Offer</span>");
        }
        kbo_window_text_appendf(buffer, "</span></td><td class='roPo'>%s</td>", kbo_futures_ui_position_label(row->player_ptr));
        kbo_webview_append_player_name_cell(buffer, player_name, row->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, seller_name);
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_roster_nation_cell(buffer, row->nation_id, kbo_hub_nation_flag_asset_path);
        kbo_window_text_appendf(
            buffer,
            "<td class='roAge'>%u</td><td class='roSlot'>",
            (uint32_t)row->age);
        kbo_html_append_escaped(buffer, row->slot_label);
        kbo_window_text_appendf(buffer, "</td><td class='roCash' data-sort-value='%d'>", row->cash_cost);
        kbo_html_append_escaped(buffer, cash_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roScore' data-sort-value='%lld'>%lld</td><td class='roStatus'>",
            (long long)row->request_score,
            (long long)row->request_score);
        kbo_html_append_escaped(buffer, status);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}

static void kbo_futures_ui_append_pending_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    uint32_t buyer_team_id = kbo_futures_ui_resolve_buyer_team_id(selected_team_id);
    KboIndependentAcquisitionUiRequestRow rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    KboIndependentAcquisitionUiContext context;
    kbo_independent_acquisition_ui_context(buyer_team_id, &context);
    int count = kbo_independent_acquisition_ui_load_pending_rows(
        buyer_team_id,
        rows,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights futuresRights'>");
    kbo_futures_ui_append_context_bar(buffer, &context, count, "PENDING");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable futuresPendingTable'><thead><tr>"
        "<th class='roDate' data-sort-type='number'>Date</th><th class='roPo' data-sort-type='text'>PO</th>"
        "<th class='roName' data-sort-type='text'>Name</th><th class='roClub' data-sort-type='text'>Independent Club</th>"
        "<th class='roNat' data-sort-type='text'>Nation</th><th class='roSlot' data-sort-type='text'>Slot</th>"
        "<th class='roCash' data-sort-type='number'>Cash</th><th class='roScore' data-sort-type='number'>Score</th>"
        "<th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    if (!context.buyer_valid) {
        kbo_futures_ui_append_empty_row(buffer, 9, "Select a KBO club first.");
    } else if (count <= 0) {
        kbo_futures_ui_append_empty_row(buffer, 9, "No pending independent club offer.");
    }
    for (int i = 0; i < count; i++) {
        KboIndependentAcquisitionUiRequestRow* row = &rows[i];
        char date_text[16] = {0};
        char player_name[96] = {0};
        char seller_name[96] = {0};
        char cash_text[32] = {0};
        kbo_futures_ui_format_yyyymmdd(row->date, date_text, sizeof(date_text));
        kbo_futures_ui_copy_player_name(row->player_ptr, row->player_id, player_name, sizeof(player_name));
        kbo_futures_ui_copy_team_name(row->seller_team_id, seller_name, sizeof(seller_name));
        kbo_futures_ui_format_cash(row->cash_cost, cash_text, sizeof(cash_text));

        kbo_window_text_appendf(
            buffer,
            "<tr><td class='roDate' data-sort-value='%u'>",
            row->date);
        kbo_html_append_escaped(buffer, date_text);
        kbo_window_text_appendf(buffer, "</td><td class='roPo'>%s</td>", kbo_futures_ui_position_label(row->player_ptr));
        kbo_webview_append_player_name_cell(buffer, player_name, row->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, seller_name);
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_roster_nation_cell(buffer, row->nation_id, kbo_hub_nation_flag_asset_path);
        kbo_window_text_appendf(buffer, "<td class='roSlot'>");
        kbo_html_append_escaped(buffer, row->slot_label);
        kbo_window_text_appendf(buffer, "</td><td class='roCash' data-sort-value='%d'>", row->cash_cost);
        kbo_html_append_escaped(buffer, cash_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roScore' data-sort-value='%lld'>%lld</td><td class='roStatus'>PENDING</td></tr>",
            (long long)row->request_score,
            (long long)row->request_score);
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}

static const char* kbo_futures_ui_result_label(const KboIndependentAcquisitionUiResultRow* row)
{
    if (row == NULL) {
        return "-";
    }
    if (row->winning_buyer_team_id == row->request.buyer_team_id && row->transferred) {
        return "ACQUIRED";
    }
    if (row->winning_buyer_team_id == row->request.buyer_team_id) {
        return "FAILED";
    }
    return "LOST";
}

static void kbo_futures_ui_append_result_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    uint32_t buyer_team_id = kbo_futures_ui_resolve_buyer_team_id(selected_team_id);
    KboIndependentAcquisitionUiResultRow rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    KboIndependentAcquisitionUiContext context;
    kbo_independent_acquisition_ui_context(buyer_team_id, &context);
    int count = kbo_independent_acquisition_ui_load_result_rows(
        buyer_team_id,
        rows,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights futuresRights'>");
    kbo_futures_ui_append_context_bar(buffer, &context, count, "RESULT");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable futuresResultTable'><thead><tr>"
        "<th class='roDate' data-sort-type='number'>Decision</th><th class='roResult' data-sort-type='text'>Result</th>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roClub' data-sort-type='text'>Independent Club</th><th class='roTeam' data-sort-type='text'>Winner</th>"
        "<th class='roCash' data-sort-type='number'>Cash</th><th class='roScore' data-sort-type='number'>Score</th>"
        "</tr></thead><tbody>");

    if (!context.buyer_valid) {
        kbo_futures_ui_append_empty_row(buffer, 8, "Select a KBO club first.");
    } else if (count <= 0) {
        kbo_futures_ui_append_empty_row(buffer, 8, "No independent club offer result.");
    }
    for (int i = 0; i < count; i++) {
        KboIndependentAcquisitionUiResultRow* row = &rows[i];
        char date_text[16] = {0};
        char player_name[96] = {0};
        char seller_name[96] = {0};
        char winner_name[96] = {0};
        char cash_text[32] = {0};
        const char* result = kbo_futures_ui_result_label(row);
        kbo_futures_ui_format_yyyymmdd(row->decision_date, date_text, sizeof(date_text));
        kbo_futures_ui_copy_player_name(row->request.player_ptr, row->request.player_id, player_name, sizeof(player_name));
        kbo_futures_ui_copy_team_name(row->request.seller_team_id, seller_name, sizeof(seller_name));
        kbo_futures_ui_copy_team_name(row->winning_buyer_team_id, winner_name, sizeof(winner_name));
        kbo_futures_ui_format_cash(row->request.cash_cost, cash_text, sizeof(cash_text));

        kbo_window_text_appendf(
            buffer,
            "<tr><td class='roDate' data-sort-value='%u'>",
            row->decision_date);
        kbo_html_append_escaped(buffer, date_text);
        kbo_window_text_appendf(buffer, "</td><td class='roResult'>");
        kbo_html_append_escaped(buffer, result);
        kbo_window_text_appendf(buffer, "</td><td class='roPo'>%s</td>", kbo_futures_ui_position_label(row->request.player_ptr));
        kbo_webview_append_player_name_cell(buffer, player_name, row->request.player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, seller_name);
        kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
        kbo_html_append_escaped(buffer, winner_name);
        kbo_window_text_appendf(buffer, "</td><td class='roCash' data-sort-value='%d'>", row->request.cash_cost);
        kbo_html_append_escaped(buffer, cash_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roScore' data-sort-value='%lld'>%lld</td></tr>",
            (long long)row->request.request_score,
            (long long)row->request.request_score);
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}

void kbo_webview_append_futures_league_view(
    KboWindowTextBuffer* buffer,
    int selected_futures_subview,
    uint32_t selected_team_id)
{
    if (buffer == NULL) {
        return;
    }
    if (selected_futures_subview == KBO_HUB_FUTURES_SUBVIEW_PENDING) {
        kbo_futures_ui_append_pending_view(buffer, selected_team_id);
        return;
    }
    if (selected_futures_subview == KBO_HUB_FUTURES_SUBVIEW_RESULT) {
        kbo_futures_ui_append_result_view(buffer, selected_team_id);
        return;
    }
    kbo_futures_ui_append_offer_view(buffer, selected_team_id);
}
