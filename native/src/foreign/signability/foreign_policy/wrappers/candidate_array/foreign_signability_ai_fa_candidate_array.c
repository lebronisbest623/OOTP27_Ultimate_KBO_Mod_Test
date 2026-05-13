#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../runtime_memory/runtime_memory.h"
#include "foreign_signability_ai_fa_candidate_array.h"

#define KBO_AI_FA_STATUS_CANDIDATE_ARRAY_HARD_LIMIT 8192

int kbo_ai_fa_status_candidate_slot_accessible(uintptr_t candidate_array, int32_t index)
{
    if (candidate_array == 0 || index < 0 || index >= KBO_AI_FA_STATUS_CANDIDATE_ARRAY_HARD_LIMIT) {
        return 0;
    }
    return memory_range_readable(
        (void*)(candidate_array + ((uintptr_t)index * sizeof(uintptr_t))),
        sizeof(uintptr_t));
}

int kbo_ai_fa_status_candidate_array_contains(
    uintptr_t candidate_array,
    int32_t count,
    uintptr_t player_ptr)
{
    if (candidate_array == 0 || player_ptr == 0 || count <= 0) {
        return 0;
    }

    int32_t limit = count;
    if (limit > KBO_AI_FA_STATUS_CANDIDATE_ARRAY_HARD_LIMIT) {
        limit = KBO_AI_FA_STATUS_CANDIDATE_ARRAY_HARD_LIMIT;
    }

    for (int32_t i = 0; i < limit; i++) {
        if (!kbo_ai_fa_status_candidate_slot_accessible(candidate_array, i)) {
            break;
        }
        if (*(uintptr_t*)(candidate_array + ((uintptr_t)i * sizeof(uintptr_t))) == player_ptr) {
            return 1;
        }
    }
    return 0;
}

int32_t kbo_ai_fa_status_insert_candidate_ptr(
    uintptr_t frame_ptr,
    uintptr_t candidate_array,
    int32_t insert_index,
    uintptr_t player_ptr)
{
    if (player_ptr == 0 || !kbo_ai_fa_status_candidate_slot_accessible(candidate_array, insert_index)) {
        return insert_index;
    }

    *(uintptr_t*)(candidate_array + ((uintptr_t)insert_index * sizeof(uintptr_t))) = player_ptr;
    if (frame_ptr > OOTP27_AI_FA_STATUS_FRAME_INSERT_COUNT_DELTA) {
        *(int32_t*)(frame_ptr - OOTP27_AI_FA_STATUS_FRAME_INSERT_COUNT_DELTA) = insert_index + 1;
    }
    return insert_index + 1;
}
