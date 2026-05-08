static uint8_t* kbo_choose_amateur_assignment_team(
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    uint8_t* out_target_reputation)
{
    if (out_target_reputation != NULL) {
        *out_target_reputation = 0u;
    }
    if (player == NULL || league_id == 0u || current_team_id == 0u || quality_score <= 0) {
        return NULL;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1) {
        return NULL;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int32_t target = kbo_amateur_assignment_target_reputation(league_id, quality_score);
    if (out_target_reputation != NULL) {
        *out_target_reputation = (uint8_t)target;
    }

    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);
    if (player_tier > 0 && target <= (int32_t)current_reputation) {
        return NULL;
    }
    int max_team_tier = -1;
    for (int i = 0; i < count; i++) {
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        int team_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
        if (team_tier > max_team_tier) {
            max_team_tier = team_tier;
        }
    }
    int assignment_tier = kbo_amateur_assignment_effective_player_tier(player_tier, max_team_tier);

    int32_t current_player_count = 0;
    int32_t target_player_count = 0x7fffffff;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            current_player_count = candidates[i].player_count;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        if (kbo_amateur_assignment_candidate_weight(
                league_id,
                quality_score,
                assignment_tier,
                target,
                candidates[i].reputation,
                candidates[i].player_count,
                -1) == 0u) {
            continue;
        }
        if (candidates[i].player_count < target_player_count) {
            target_player_count = candidates[i].player_count;
        }
    }
    if (target_player_count == 0x7fffffff) {
        return NULL;
    }

    uint32_t total_weight = 0u;
    uint32_t current_weight = kbo_amateur_assignment_candidate_weight(
        league_id,
        quality_score,
        assignment_tier,
        target,
        current_reputation,
        current_player_count,
        target_player_count);
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            continue;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        uint32_t weight = kbo_amateur_assignment_candidate_weight(
            league_id,
            quality_score,
            assignment_tier,
            target,
            candidates[i].reputation,
            candidates[i].player_count,
            target_player_count);
        if (weight == 0u) {
            continue;
        }
        total_weight += weight;
    }

    if (total_weight == 0u) {
        return NULL;
    }

    if (current_weight > 0u) {
        return NULL;
    }

    uint32_t pick = kbo_amateur_assignment_pick_hash(player_id, league_id, quality_score, target) % total_weight;
    uint32_t seen = 0u;
    int fallback_index = -1;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            continue;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        uint32_t weight = kbo_amateur_assignment_candidate_weight(
            league_id,
            quality_score,
            assignment_tier,
            target,
            candidates[i].reputation,
            candidates[i].player_count,
            target_player_count);
        if (weight == 0u) {
            continue;
        }
        fallback_index = i;
        if (pick < seen + weight) {
            return candidates[i].team;
        }
        seen += weight;
    }

    return fallback_index >= 0 ? candidates[fallback_index].team : NULL;
}
