static int kbo_hub_league_id_list_index(const uint32_t* league_ids, int league_count, uint32_t league_id)
{
    if (league_ids == NULL || league_id == 0) {
        return -1;
    }
    for (int i = 0; i < league_count; i++) {
        if (league_ids[i] == league_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_hub_collect_visible_league_ids(uint32_t* league_ids, int max_leagues)
{
    if (league_ids == NULL || max_leagues <= 0) {
        return 0;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return 0;
    }

    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < max_leagues; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0 || kbo_hub_league_id_list_index(league_ids, league_count, league_id) >= 0) {
            continue;
        }

        league_ids[league_count++] = league_id;
    }

    return league_count;
}

static int kbo_hub_count_cached_league_ids(const uint32_t* league_ids, int league_count)
{
    int count = 0;
    for (int i = 0; i < league_count; i++) {
        if (kbo_hub_find_league_display_cache_slot(league_ids[i]) >= 0) {
            count++;
        }
    }
    return count;
}

