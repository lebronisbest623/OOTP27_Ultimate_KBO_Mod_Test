static uintptr_t kbo_amateur_team_add_player_reroute_before_original(uintptr_t team_ptr, uintptr_t player_ptr, const char* source)
{
    if (read_kbo_localappdata_flag_file("disable_amateur_assignment_reroute.txt")
            || team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return team_ptr;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = kbo_resolve_amateur_assignment_league_id_for_team_ptr(team);
    if (team_league_id == 0u) {
        return team_ptr;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if (player_id == 0u || !kbo_amateur_player_age_eligible(team_league_id, age)
            || kbo_player_is_draft_pool_candidate(player)
            || kbo_amateur_assignment_already_processed(player_id, team_id)) {
        return team_ptr;
    }

    uint8_t current_reputation = 70u;
    if (!kbo_find_amateur_team_reputation_by_memory_team(team_league_id, team, &current_reputation)) {
        return team_ptr;
    }

    if (team_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || team_league_id == KBO_COLLEGE_LEAGUE_ID) {
        int32_t current_player_count = -1;
        int32_t current_hitter_count = -1;
        kbo_amateur_assignment_find_candidate_info(
            team_league_id,
            team_id,
            NULL,
            NULL,
            &current_player_count,
            &current_hitter_count);
        int32_t source_min_players = team_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID
            ? KBO_HIGH_SCHOOL_ASSIGNMENT_SOURCE_MIN_PLAYERS
            : KBO_COLLEGE_ASSIGNMENT_SOURCE_MIN_PLAYERS;
        if (current_player_count >= 0 && current_player_count < source_min_players) {
            return team_ptr;
        }
        if (kbo_amateur_player_is_hitter(player)
                && current_hitter_count >= 0
                && current_hitter_count < KBO_AMATEUR_ASSIGNMENT_SOURCE_MIN_HITTERS) {
            return team_ptr;
        }
    }

    int32_t quality_score = kbo_amateur_quality_score(player);
    uint8_t target_reputation = 0u;
    uint8_t* target_team = kbo_choose_amateur_assignment_team(
        player,
        team_league_id,
        team_id,
        current_reputation,
        quality_score,
        &target_reputation);
    if (target_team == NULL || target_team == team) {
        int player_tier = kbo_amateur_assignment_player_tier(team_league_id, quality_score);
        int from_tier = kbo_amateur_assignment_team_tier(team_league_id, current_reputation);
        int32_t from_player_count = -1;
        kbo_amateur_assignment_find_candidate_info(
            team_league_id,
            team_id,
            NULL,
            NULL,
            &from_player_count,
            NULL);
        kbo_amateur_assignment_append_debug_csv(
            target_team == team ? "pre_keep_same_tier_slot" : "pre_keep_no_candidate",
            source,
            player_id,
            team_league_id,
            age,
            quality_score,
            player_tier,
            team_id,
            current_reputation,
            from_tier,
            from_player_count,
            team_id,
            current_reputation,
            from_tier,
            from_player_count,
            from_player_count,
            target_reputation,
            -1,
            0u,
            0u);
        return team_ptr;
    }

    uint32_t target_team_id = *(uint32_t*)(target_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint8_t actual_target_reputation = 70u;
    kbo_find_amateur_team_reputation_by_memory_team(team_league_id, target_team, &actual_target_reputation);
    int player_tier = kbo_amateur_assignment_player_tier(team_league_id, quality_score);
    int from_tier = kbo_amateur_assignment_team_tier(team_league_id, current_reputation);
    int to_tier = kbo_amateur_assignment_team_tier(team_league_id, actual_target_reputation);
    int32_t from_player_count = -1;
    int32_t to_player_count = -1;
    kbo_amateur_assignment_find_candidate_info(
        team_league_id,
        team_id,
            NULL,
            NULL,
            &from_player_count,
            NULL);
        kbo_amateur_assignment_find_candidate_info(
            team_league_id,
            target_team_id,
            NULL,
            NULL,
            &to_player_count,
            NULL);
    int32_t target_player_count = to_player_count;

    kbo_amateur_assignment_append_debug_csv(
        "pre_reroute_decision",
        source,
        player_id,
        team_league_id,
        age,
        quality_score,
        player_tier,
        team_id,
        current_reputation,
        from_tier,
        from_player_count,
        target_team_id,
        actual_target_reputation,
        to_tier,
        to_player_count,
        target_player_count,
        target_reputation,
        -1,
        0u,
        0u);

    static volatile LONG reroute_before_log_count = 0;
    LONG slot = InterlockedIncrement(&reroute_before_log_count);
    int verbose_assignment_log = read_kbo_localappdata_flag_file("enable_amateur_assignment_verbose_log.txt");
    if (verbose_assignment_log || slot <= 30) {
        append_logf(
            "amateur assignment pre-reroute team-add source=%s player=%u league=%u age=%d score=%d target_rep=%u team=%u(rep=%u)->%u(rep=%u)",
            source != NULL ? source : "",
            player_id,
            team_league_id,
            (int)age,
            quality_score,
            (uint32_t)target_reputation,
            team_id,
            (uint32_t)current_reputation,
            target_team_id,
            (uint32_t)actual_target_reputation);
    } else if (slot == 31) {
        append_log_line("amateur assignment pre-reroute log suppressed after 30 players; create enable_amateur_assignment_verbose_log.txt for full logging");
    }

    return (uintptr_t)target_team;
}
