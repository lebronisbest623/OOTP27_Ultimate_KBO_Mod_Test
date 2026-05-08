#include "league_context_lookup.h"

static uint32_t kbo_find_current_kbo_league_year(void)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    return kbo_current_date_is_valid(&year, &month, &day) ? year : 0;
}

int kbo_league_candidate_matches_id(uintptr_t candidate, uint32_t league_id, uint32_t id_offset)
{
    if (candidate == 0 || league_id == 0
            || !memory_range_readable((void*)candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
            || !memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    uint32_t candidate_id = *(uint32_t*)(candidate + id_offset);
    if (candidate_id != league_id) {
        return 0;
    }

    uint32_t league_year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (league_year < 1982u || league_year > 2200u) {
        return 0;
    }
    uint32_t current_year = kbo_find_current_kbo_league_year();
    if (current_year >= 1982u && current_year <= 2200u
            && (league_year + 1u < current_year || league_year > current_year + 1u)) {
        return 0;
    }

    uint8_t phase = *(uint8_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    if (phase > 4u) {
        return 0;
    }

    uint32_t phase_year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
    if (phase_year != 0u && (phase_year < 1982u || phase_year > 2200u)) {
        return 0;
    }

    return 1;
}

static int kbo_league_scan_protect_allows_read(DWORD protect)
{
    if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0) {
        return 0;
    }
    protect &= 0xffu;
    return protect == PAGE_READONLY
        || protect == PAGE_READWRITE
        || protect == PAGE_WRITECOPY;
}
