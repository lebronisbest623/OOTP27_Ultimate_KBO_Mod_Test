#include "../api/league_context_lookup.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"

uintptr_t kbo_find_league_ptr_from_global_vectors(uint32_t league_id)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0 || league_id == 0) {
        return 0;
    }

    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u,
        0xe0u, 0xe8u, 0xf0u, 0xf8u, 0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };

    for (size_t i = 0; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        uint32_t vec_off = league_vec_offsets[i];
        uint32_t cnt_off = vec_off + 8u;
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }

        uintptr_t candidate_vec = *(uintptr_t*)(global + vec_off);
        int32_t candidate_count = *(int32_t*)(global + cnt_off);
        if (candidate_vec == 0 || candidate_count <= 0 || candidate_count > 10000
                || !memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }

        for (int32_t j = 0; j < candidate_count; j++) {
            uintptr_t league_candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            if (league_candidate == 0 || !memory_range_readable((void*)league_candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 12u)) {
                continue;
            }
            uint32_t legacy_id = *(uint32_t*)(league_candidate + OOTP27_KBO_LEAGUE_ID_OFFSET);
            uint32_t alternate_id = *(uint32_t*)(league_candidate + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
            if (legacy_id != league_id && alternate_id != league_id) {
                continue;
            }
            if (!kbo_league_candidate_matches_id(league_candidate, league_id,
                    legacy_id == league_id ? OOTP27_KBO_LEAGUE_ID_OFFSET : OOTP27_KBO_LEAGUE_ID_OFFSET + 8u)) {
                continue;
            }
            return league_candidate;
        }
    }

    return 0;
}
