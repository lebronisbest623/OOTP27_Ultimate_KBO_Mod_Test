#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "league_context_lookup.h"
#include "../../bootstrap/ootp_offsets.h"
#include "../../runtime_memory/runtime_memory.h"

uintptr_t kbo_find_league_ptr_from_id(uint32_t league_id)
{
    uintptr_t league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
    if (league_ptr != 0) {
        return league_ptr;
    }

    return kbo_find_league_ptr_by_memory_scan(league_id);
}

uint32_t kbo_find_league_year_from_id(uint32_t league_id)
{
    uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
    if (league_ptr == 0
            || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    return *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
}

uint32_t kbo_find_league_year_from_id_no_scan(uint32_t league_id)
{
    uintptr_t league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
    if (league_ptr == 0
            || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    return *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
}
