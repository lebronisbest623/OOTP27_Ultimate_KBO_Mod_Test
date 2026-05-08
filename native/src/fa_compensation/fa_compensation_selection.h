#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_SELECTION_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_SELECTION_H_

#include <stdint.h>

#include "fa_compensation_state.h"

int kbo_select_fa_compensation_player_from_candidates(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    KboFaProtectedCandidate* out_selected,
    int* out_unprotected_count);
int kbo_fa_compensation_ai_prefers_cash_only(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    int unprotected_count);
int kbo_manual_select_fa_compensation_player(
    uint32_t fa_player_id,
    uint32_t selected_player_id,
    const char* source);
int kbo_manual_select_fa_compensation_cash_only(uint32_t fa_player_id, const char* source);

#endif
