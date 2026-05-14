#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_HISTORY_INDEPENDENT_ACQUISITION_UI_HISTORY_INTERNAL_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_HISTORY_INDEPENDENT_ACQUISITION_UI_HISTORY_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../independent_acquisition_ui.h"

typedef struct KboIndependentAcquisitionUiDecisionRow {
    uint32_t date;
    uint32_t season;
    uint32_t seller_team_id;
    uint32_t player_id;
    uint32_t buyer_team_id;
    int32_t value_score;
    int32_t cash_cost;
    int32_t old_cash;
    int32_t new_cash;
    int64_t request_score;
    uint8_t transferred;
} KboIndependentAcquisitionUiDecisionRow;

char* kbo_independent_acquisition_ui_read_text_file(const char* filename, DWORD* out_read);
int kbo_independent_acquisition_ui_parse_request_line(
    const char* line,
    KboIndependentAcquisitionUiRequestRow* out);
int kbo_independent_acquisition_ui_parse_decision_line(
    const char* line,
    KboIndependentAcquisitionUiDecisionRow* out);

#endif
