#ifndef KBOFIX_SRC_FA_COMPENSATION_DUE_FA_COMPENSATION_DUE_ORTOOLS_H_
#define KBOFIX_SRC_FA_COMPENSATION_DUE_FA_COMPENSATION_DUE_ORTOOLS_H_

#include "../protection/fa_compensation_protected_lists.h"
#include "../records/fa_compensation_records.h"

int kbo_fa_compensation_apply_ortools_order(
    KboFaCompensationRecord* rec,
    KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source);

#endif
