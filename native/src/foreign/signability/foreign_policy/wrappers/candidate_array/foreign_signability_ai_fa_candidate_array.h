#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_AI_FA_CANDIDATE_ARRAY_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_AI_FA_CANDIDATE_ARRAY_H_

#include <stdint.h>

int kbo_ai_fa_status_candidate_slot_accessible(uintptr_t candidate_array, int32_t index);
int kbo_ai_fa_status_candidate_array_contains(
    uintptr_t candidate_array,
    int32_t count,
    uintptr_t player_ptr);
int32_t kbo_ai_fa_status_insert_candidate_ptr(
    uintptr_t frame_ptr,
    uintptr_t candidate_array,
    int32_t insert_index,
    uintptr_t player_ptr);

#endif
