#ifndef KBOFIX_SRC_FA_COMPENSATION_PROTECTION_CACHE_H_
#define KBOFIX_SRC_FA_COMPENSATION_PROTECTION_CACHE_H_

#include "../fa_compensation_protection_score.h"

#define KBO_FA_PROTECTION_TEAM_CACHE_CANDIDATE_MAX (KBO_FA_COMPENSATION_PROTECTED_LIST_MAX + 1)

int kbo_fa_materialize_protection_candidates(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* base_candidates,
    int base_candidate_count,
    const KboFaProtectedCandidate* base_auto_protected,
    int base_auto_count,
    KboFaProtectedCandidate* candidates,
    int max_candidates,
    KboFaProtectedCandidate* auto_protected,
    int max_auto_protected);
int kbo_fa_try_materialize_cached_protection_candidates(
    const KboFaCompensationRecord* rec,
    uintptr_t player_vector,
    int32_t player_count,
    KboFaProtectedCandidate* candidates,
    int max_candidates,
    KboFaProtectedCandidate* auto_protected,
    int max_auto_protected);
void kbo_fa_store_protection_candidate_cache(
    const KboFaCompensationRecord* rec,
    uintptr_t player_vector,
    int32_t player_count,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const KboFaProtectedCandidate* auto_protected,
    int auto_count);

#endif