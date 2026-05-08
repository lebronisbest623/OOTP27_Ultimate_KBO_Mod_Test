#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_PROTECTED_LISTS_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_PROTECTED_LISTS_H_

#include <stdint.h>

#include "fa_compensation_state.h"

int kbo_persist_fa_compensation_protected_list(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source);
int kbo_persist_fa_compensation_protection_debug(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const KboFaProtectedCandidate* auto_protected,
    int auto_protected_count,
    const char* source);

#endif
