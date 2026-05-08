static int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age);

static int kbo_amateur_player_is_hitter(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))) {
        return 0;
    }
    return player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] != 1u;
}

static int32_t kbo_amateur_assignment_target_max_players(uint32_t league_id)
{
    return league_id == KBO_HIGH_SCHOOL_LEAGUE_ID
        ? KBO_HIGH_SCHOOL_ASSIGNMENT_TARGET_MAX_PLAYERS
        : KBO_COLLEGE_ASSIGNMENT_TARGET_MAX_PLAYERS;
}

static void kbo_read_amateur_quality_fields(
    uint8_t* player,
    int16_t* out_overall,
    int16_t* out_talent,
    int16_t* out_ratings,
    int16_t* out_career)
{
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_talent != NULL) { *out_talent = 0; }
    if (out_ratings != NULL) { *out_ratings = 0; }
    if (out_career != NULL) { *out_career = 0; }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET + sizeof(int16_t))) {
        return;
    }
    if (out_overall != NULL) { *out_overall = *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET); }
    if (out_talent != NULL) { *out_talent = *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET); }
    if (out_ratings != NULL) { *out_ratings = *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET); }
    if (out_career != NULL) { *out_career = *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET); }
}

static int32_t kbo_amateur_quality_score(uint8_t* player)
{
    int16_t overall = 0;
    int16_t talent = 0;
    int16_t ratings = 0;
    int16_t career = 0;
    kbo_read_amateur_quality_fields(player, &overall, &talent, &ratings, &career);
    return overall + talent + ratings + career;
}

static int32_t kbo_amateur_assignment_target_reputation(uint32_t league_id, int32_t quality_score)
{
    int32_t target = 50;
    if (quality_score >= 2300) {
        target = 92;
    } else if (quality_score >= 1800) {
        target = 84;
    } else if (quality_score >= 1350) {
        target = 72;
    } else if (quality_score >= 950) {
        target = 58;
    } else if (quality_score >= 600) {
        target = 45;
    } else {
        target = 35;
    }

    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        target -= 10;
    }
    if (target < 25) {
        target = 25;
    } else if (target > 95) {
        target = 95;
    }
    return target;
}

static uint32_t kbo_amateur_assignment_pick_hash(uint32_t player_id, uint32_t league_id, int32_t quality_score, int32_t target)
{
    uint32_t hash = 2166136261u;
    hash = (hash ^ player_id) * 16777619u;
    hash = (hash ^ league_id) * 16777619u;
    hash = (hash ^ (uint32_t)quality_score) * 16777619u;
    hash = (hash ^ (uint32_t)target) * 16777619u;
    hash = (hash ^ 0x9e3779b9u) * 16777619u;
    return hash;
}

static int kbo_amateur_assignment_target_rejected(uint32_t league_id, uint32_t team_id)
{
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }
    LONG count = InterlockedCompareExchange(&g_kbo_amateur_assignment_rejected_target_count, 0, 0);
    if (count > KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX) {
        count = KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX;
    }
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_amateur_assignment_rejected_targets[i].player_id == league_id
                && g_kbo_amateur_assignment_rejected_targets[i].team_id == team_id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_amateur_assignment_mark_rejected_target(uint32_t league_id, uint32_t team_id)
{
    if (league_id == 0u || team_id == 0u || kbo_amateur_assignment_target_rejected(league_id, team_id)) {
        return;
    }
    LONG slot = InterlockedIncrement(&g_kbo_amateur_assignment_rejected_target_count) - 1;
    if (slot < 0 || slot >= KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX) {
        return;
    }
    g_kbo_amateur_assignment_rejected_targets[slot].player_id = league_id;
    g_kbo_amateur_assignment_rejected_targets[slot].team_id = team_id;
}

static int kbo_amateur_assignment_team_tier(uint32_t league_id, uint8_t reputation)
{
    int32_t rep = (int32_t)reputation;
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        if (rep >= 68) { return 5; }
        if (rep >= 55) { return 4; }
        if (rep >= 42) { return 3; }
        if (rep >= 28) { return 2; }
        if (rep >= 15) { return 1; }
        return 0;
    }
    if (rep >= 90) { return 5; }
    if (rep >= 80) { return 4; }
    if (rep >= 68) { return 3; }
    if (rep >= 56) { return 2; }
    if (rep >= 48) { return 1; }
    return 0;
}

static int kbo_amateur_assignment_player_tier(uint32_t league_id, int32_t quality_score)
{
    (void)league_id;
    int tier = 0;
    if (quality_score >= 2300) {
        tier = 5;
    } else if (quality_score >= 1800) {
        tier = 4;
    } else if (quality_score >= 1350) {
        tier = 3;
    } else if (quality_score >= 950) {
        tier = 2;
    } else if (quality_score >= 600) {
        tier = 1;
    } else {
        tier = 0;
    }
    return tier;
}

static int kbo_amateur_assignment_tier_allowed(int player_tier, int team_tier)
{
    return player_tier == team_tier;
}

static int kbo_amateur_assignment_effective_player_tier(int player_tier, int max_team_tier)
{
    if (max_team_tier < 0) {
        return player_tier;
    }
    if (player_tier > max_team_tier) {
        return max_team_tier;
    }
    return player_tier;
}
