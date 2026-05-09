#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_DECISIONS_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_DECISIONS_H_

#include <stdint.h>

#include "../state/fa_compensation_state.h"

int kbo_load_fa_compensation_protection_debug_rows(
    KboFaCompensationProtectionDebugRow* rows,
    int max_rows);
int kbo_persist_fa_compensation_player_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const KboFaProtectedCandidate* selected,
    int unprotected_candidate_count,
    const char* source);
int kbo_persist_fa_compensation_cash_only_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const char* source);
int kbo_load_latest_fa_compensation_decision(
    uint32_t fa_player_id,
    KboFaCompensationDecisionRow* out);

#endif
