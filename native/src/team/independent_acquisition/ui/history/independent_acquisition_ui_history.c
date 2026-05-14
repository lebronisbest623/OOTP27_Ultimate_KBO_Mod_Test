#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ui_history_internal.h"
#include "../../ai/independent_acquisition_ai_internal.h"

#include <stdlib.h>
#include <string.h>

static const KboIndependentAcquisitionUiDecisionRow*
kbo_independent_acquisition_ui_find_decision(
    const KboIndependentAcquisitionUiDecisionRow* decisions,
    int decision_count,
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (decisions == NULL || decision_count <= 0) {
        return NULL;
    }
    for (int i = 0; i < decision_count; i++) {
        const KboIndependentAcquisitionUiDecisionRow* row = &decisions[i];
        if (row->season == season
                && row->seller_team_id == seller_team_id
                && row->player_id == player_id) {
            return row;
        }
    }
    return NULL;
}

static int kbo_independent_acquisition_ui_load_decision_rows(
    uint32_t season,
    KboIndependentAcquisitionUiDecisionRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);
    DWORD read = 0u;
    char* buffer = kbo_independent_acquisition_ui_read_text_file(
        KBO_INDEPENDENT_ACQUISITION_DECISION_FILE,
        &read);
    if (buffer == NULL) {
        return 0;
    }

    int count = 0;
    char* cursor = buffer;
    char* end = buffer + read;
    while (cursor < end && count < max_rows) {
        char* line_end = cursor;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') {
            line_end++;
        }
        char saved = *line_end;
        *line_end = '\0';
        KboIndependentAcquisitionUiDecisionRow row;
        if (kbo_independent_acquisition_ui_parse_decision_line(cursor, &row)
                && row.season == season) {
            out_rows[count++] = row;
        }
        *line_end = saved;
        while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
            line_end++;
        }
        cursor = line_end;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return count;
}

static int kbo_independent_acquisition_ui_request_row_cmp_desc(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiRequestRow* left = (const KboIndependentAcquisitionUiRequestRow*)a;
    const KboIndependentAcquisitionUiRequestRow* right = (const KboIndependentAcquisitionUiRequestRow*)b;
    if (left->date < right->date) { return 1; }
    if (left->date > right->date) { return -1; }
    if (left->request_score < right->request_score) { return 1; }
    if (left->request_score > right->request_score) { return -1; }
    return 0;
}

static int kbo_independent_acquisition_ui_result_row_cmp_desc(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiResultRow* left = (const KboIndependentAcquisitionUiResultRow*)a;
    const KboIndependentAcquisitionUiResultRow* right = (const KboIndependentAcquisitionUiResultRow*)b;
    if (left->decision_date < right->decision_date) { return 1; }
    if (left->decision_date > right->decision_date) { return -1; }
    if (left->request.date < right->request.date) { return 1; }
    if (left->request.date > right->request.date) { return -1; }
    return 0;
}

int kbo_independent_acquisition_ui_load_pending_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiRequestRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        return 0;
    }

    DWORD read = 0u;
    char* buffer = kbo_independent_acquisition_ui_read_text_file(
        KBO_INDEPENDENT_ACQUISITION_REQUEST_FILE,
        &read);
    if (buffer == NULL) {
        return 0;
    }

    int count = 0;
    char* cursor = buffer;
    char* end = buffer + read;
    while (cursor < end && count < max_rows) {
        char* line_end = cursor;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') {
            line_end++;
        }
        char saved = *line_end;
        *line_end = '\0';
        KboIndependentAcquisitionUiRequestRow row;
        if (kbo_independent_acquisition_ui_parse_request_line(cursor, &row)
                && row.season == context.season
                && row.buyer_team_id == buyer_team_id
                && !kbo_independent_acquisition_decision_exists(row.season, row.seller_team_id, row.player_id)) {
            out_rows[count++] = row;
        }
        *line_end = saved;
        while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
            line_end++;
        }
        cursor = line_end;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_request_row_cmp_desc);
    }
    return count;
}

int kbo_independent_acquisition_ui_load_result_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiResultRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        return 0;
    }

    KboIndependentAcquisitionUiDecisionRow decisions[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    int decision_count = kbo_independent_acquisition_ui_load_decision_rows(
        context.season,
        decisions,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS);
    if (decision_count <= 0) {
        return 0;
    }

    DWORD read = 0u;
    char* buffer = kbo_independent_acquisition_ui_read_text_file(
        KBO_INDEPENDENT_ACQUISITION_REQUEST_FILE,
        &read);
    if (buffer == NULL) {
        return 0;
    }

    int count = 0;
    char* cursor = buffer;
    char* end = buffer + read;
    while (cursor < end && count < max_rows) {
        char* line_end = cursor;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') {
            line_end++;
        }
        char saved = *line_end;
        *line_end = '\0';
        KboIndependentAcquisitionUiRequestRow request;
        if (kbo_independent_acquisition_ui_parse_request_line(cursor, &request)
                && request.season == context.season
                && request.buyer_team_id == buyer_team_id) {
            const KboIndependentAcquisitionUiDecisionRow* decision =
                kbo_independent_acquisition_ui_find_decision(
                    decisions,
                    decision_count,
                    request.season,
                    request.seller_team_id,
                    request.player_id);
            if (decision != NULL) {
                KboIndependentAcquisitionUiResultRow result;
                memset(&result, 0, sizeof(result));
                result.request = request;
                result.decision_date = decision->date;
                result.winning_buyer_team_id = decision->buyer_team_id;
                result.old_cash = decision->old_cash;
                result.new_cash = decision->new_cash;
                result.transferred = decision->transferred;
                out_rows[count++] = result;
            }
        }
        *line_end = saved;
        while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
            line_end++;
        }
        cursor = line_end;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_result_row_cmp_desc);
    }
    return count;
}
