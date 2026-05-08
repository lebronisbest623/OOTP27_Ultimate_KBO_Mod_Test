#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_PROTECTION_SCORE_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_PROTECTION_SCORE_H_

#include <stddef.h>
#include <stdint.h>

#include "fa_compensation_state.h"

int32_t kbo_fa_compensation_player_decision_score(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidate,
    char* reason,
    size_t reason_size);
int kbo_build_fa_compensation_protected_candidates(
    const KboFaCompensationRecord* rec,
    KboFaProtectedCandidate* candidates,
    int max_candidates,
    KboFaProtectedCandidate* auto_protected,
    int max_auto_protected);

#endif
