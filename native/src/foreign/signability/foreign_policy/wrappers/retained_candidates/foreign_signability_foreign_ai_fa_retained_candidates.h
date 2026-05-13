#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_AI_FA_RETAINED_CANDIDATES_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_AI_FA_RETAINED_CANDIDATES_H_

#include <stdint.h>

int32_t kbo_ai_fa_status_force_retained_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index);

#endif
