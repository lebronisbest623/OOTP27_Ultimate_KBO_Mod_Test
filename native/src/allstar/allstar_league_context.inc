/* All-Star league and team context helpers. */

static uint32_t kbo_allstar_read_u32(uint8_t* base, uint32_t offset)
{
    if (base == NULL || !memory_range_readable(base + offset, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(base + offset);
}

static uintptr_t kbo_find_allstar_league_ptr(uint32_t league_id)
{
    if (league_id == 0u) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u,
        0xe0u, 0xe8u, 0xf0u, 0xf8u, 0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };
    KboAllstarLayout layout = kbo_get_allstar_layout();

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
            if (league_candidate == 0 || !memory_range_readable((void*)league_candidate, layout.league_id_fallback_offset + 4u)) {
                continue;
            }

            uint8_t* league = (uint8_t*)league_candidate;
            uint32_t legacy_id = 0u;
            if (memory_range_readable(league + OOTP27_KBO_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
                legacy_id = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_ID_OFFSET);
            }
            uint32_t primary_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
            uint32_t fallback_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
            if (legacy_id == league_id || primary_id == league_id || fallback_id == league_id) {
                return league_candidate;
            }
        }
    }

    return 0;
}

static int kbo_allstar_league_context_enabled(uintptr_t league_ptr)
{
    if (!kbo_fix_enabled() || league_ptr == 0) {
        return 0;
    }

    if (is_kbo_historical_league_context(league_ptr)) {
        return 1;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))) {
        return 0;
    }
    if (!memory_range_readable(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET, sizeof(uint32_t))) {
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

    return 0;
}

static int kbo_allstar_team_matches_league(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (primary_league_id != 0 && team_league_id == primary_league_id) {
        return 1;
    }
    return fallback_league_id != 0 && fallback_league_id != primary_league_id && team_league_id == fallback_league_id;
}
