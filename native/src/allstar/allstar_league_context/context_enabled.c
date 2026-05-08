#include "allstar_league_context.h"

int kbo_allstar_league_context_enabled(uintptr_t league_ptr)
{
    if (!kbo_fix_enabled() || league_ptr == 0) {
        return 0;
    }

    if (is_kbo_historical_league_context(league_ptr)) {
        return 1;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))
            || !kbo_allstar_league_core_plausible(league_ptr)) {
        return 0;
    }

    uint32_t year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (year < 1982u || year > 2200u) {
        return 0;
    }

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    uint32_t primary_league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    if (primary_league_id == configured_league_id || fallback_league_id == configured_league_id) {
        return 1;
    }
    if (primary_league_id == OOTP27_KBO_MAIN_LEAGUE_ID || fallback_league_id == OOTP27_KBO_MAIN_LEAGUE_ID) {
        return 1;
    }
    if (kbo_allstar_league_has_seeded_division_split(league_ptr)) {
        return 1;
    }

    return 0;
}

int kbo_allstar_team_matches_league(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }

    return kbo_allstar_team_matches_league_ids(team, primary_league_id, fallback_league_id);
}
