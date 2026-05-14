#ifndef KBOFIX_SRC_FA_COMPENSATION_PROTECTION_CANDIDATE_SCORE_FA_COMPENSATION_CANDIDATE_SCORE_H_
#define KBOFIX_SRC_FA_COMPENSATION_PROTECTION_CANDIDATE_SCORE_FA_COMPENSATION_CANDIDATE_SCORE_H_

#include <stddef.h>
#include <stdint.h>

int kbo_fa_role_bucket(uint8_t role);
int kbo_fa_team_role_count(uint32_t team_id, int role_bucket);
int32_t kbo_fa_protection_candidate_score(
    uint8_t* player,
    uint32_t signing_team_id,
    char* reason,
    size_t reason_size);

#endif
