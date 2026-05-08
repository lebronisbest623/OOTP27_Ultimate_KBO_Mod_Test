static uint32_t kbo_amateur_assignment_candidate_weight(
    uint32_t league_id,
    int32_t quality_score,
    int assignment_tier,
    int32_t target,
    uint8_t reputation,
    int32_t player_count,
    int32_t target_player_count)
{
    int32_t target_max_players = kbo_amateur_assignment_target_max_players(league_id);
    if (target_max_players > 0 && player_count >= target_max_players) {
        return 0u;
    }
    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);
    if (assignment_tier >= 0) {
        player_tier = assignment_tier;
    }
    int team_tier = kbo_amateur_assignment_team_tier(league_id, reputation);
    if (!kbo_amateur_assignment_tier_allowed(player_tier, team_tier)) {
        return 0u;
    }
    if (target_player_count >= 0 && player_count > target_player_count) {
        return 0u;
    }

    int32_t distance = abs((int32_t)reputation - target);
    int32_t base = 72 - (distance * 3);
    if (base < 12) {
        base = 12;
    }
    uint32_t weight = (uint32_t)base;
    if (weight == 0u) {
        weight = 1u;
    }
    return weight;
}

static int kbo_amateur_assignment_collect_candidates(
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int max_candidates)
{
    if (league_id == 0u || candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int found = 0;
    for (int32_t i = 0; i < team_count && found < max_candidates; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint8_t reputation = 70u;
        if (team_id == 0u || !kbo_find_amateur_team_reputation_by_memory_team(league_id, team, &reputation)) {
            continue;
        }

        candidates[found].team = team;
        candidates[found].team_id = team_id;
        candidates[found].reputation = reputation;
        candidates[found].player_count = 0;
        found++;
    }
    return found;
}

static void kbo_amateur_assignment_refresh_player_counts(
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int candidate_count)
{
    if (league_id == 0u || candidates == NULL || candidate_count <= 0) {
        return;
    }

    for (int i = 0; i < candidate_count; i++) {
        candidates[i].player_count = 0;
        candidates[i].hitter_count = 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (!kbo_amateur_player_age_eligible(league_id, age)) {
            continue;
        }
        uint32_t team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (team_id == 0u) {
            team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        }
        if (team_id == 0u) {
            continue;
        }
        for (int c = 0; c < candidate_count; c++) {
            if (candidates[c].team_id == team_id) {
                candidates[c].player_count++;
                if (kbo_amateur_player_is_hitter(player)) {
                    candidates[c].hitter_count++;
                }
                break;
            }
        }
    }
}

static void kbo_amateur_assignment_note_player_count_delta(uint32_t league_id, uint32_t team_id, uint8_t* player, int32_t delta)
{
    if (team_id == 0u || delta == 0) {
        return;
    }
    int* count_ptr = kbo_amateur_assignment_count_ptr_for_league(league_id);
    KboAmateurAssignmentCandidate* candidates = kbo_amateur_assignment_cache_for_league(league_id);
    if (count_ptr == NULL || candidates == NULL || *count_ptr <= 0) {
        return;
    }

    kbo_lock_amateur_assignment_candidates();
    int count = *count_ptr;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            candidates[i].player_count += delta;
            if (candidates[i].player_count < 0) {
                candidates[i].player_count = 0;
            }
            if (kbo_amateur_player_is_hitter(player)) {
                candidates[i].hitter_count += delta;
                if (candidates[i].hitter_count < 0) {
                    candidates[i].hitter_count = 0;
                }
            }
            break;
        }
    }
    kbo_unlock_amateur_assignment_candidates();
}

static int kbo_amateur_assignment_get_cached_candidates(
    uint32_t league_id,
    KboAmateurAssignmentCandidate** out_candidates)
{
    if (out_candidates != NULL) {
        *out_candidates = NULL;
    }
    if (league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID) {
        return 0;
    }

    KboAmateurAssignmentCandidate* cache = kbo_amateur_assignment_cache_for_league(league_id);
    int* count_ptr = kbo_amateur_assignment_count_ptr_for_league(league_id);
    if (cache == NULL || count_ptr == NULL) {
        return 0;
    }

    if (*count_ptr >= 0) {
        if (out_candidates != NULL) {
            *out_candidates = cache;
        }
        return *count_ptr;
    }

    kbo_lock_amateur_assignment_candidates();
    if (*count_ptr < 0) {
        memset(cache, 0, sizeof(KboAmateurAssignmentCandidate) * KBO_AMATEUR_ASSIGNMENT_TEAM_MAX);
        *count_ptr = kbo_amateur_assignment_collect_candidates(
            league_id,
            cache,
            KBO_AMATEUR_ASSIGNMENT_TEAM_MAX);
        kbo_amateur_assignment_refresh_player_counts(league_id, cache, *count_ptr);
        append_logf(
            "amateur assignment candidate cache built league=%u teams=%d counts=refreshed",
            league_id,
            *count_ptr);
    }
    int count = *count_ptr;
    kbo_unlock_amateur_assignment_candidates();

    if (out_candidates != NULL) {
        *out_candidates = cache;
    }
    return count;
}

static int kbo_amateur_assignment_find_candidate_info(
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* out_reputation,
    int* out_tier,
    int32_t* out_player_count,
    int32_t* out_hitter_count)
{
    if (out_reputation != NULL) { *out_reputation = 0u; }
    if (out_tier != NULL) { *out_tier = -1; }
    if (out_player_count != NULL) { *out_player_count = -1; }
    if (out_hitter_count != NULL) { *out_hitter_count = -1; }
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 0 || candidates == NULL) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            if (out_reputation != NULL) {
                *out_reputation = candidates[i].reputation;
            }
            if (out_tier != NULL) {
                *out_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
            }
            if (out_player_count != NULL) {
                *out_player_count = candidates[i].player_count;
            }
            if (out_hitter_count != NULL) {
                *out_hitter_count = candidates[i].hitter_count;
            }
            return 1;
        }
    }
    return 0;
}
