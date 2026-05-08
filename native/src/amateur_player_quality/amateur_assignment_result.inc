static void kbo_amateur_team_add_player_note_original_success(uintptr_t team_ptr, uintptr_t player_ptr, const char* source, int original_result)
{
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }
    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = kbo_resolve_amateur_assignment_league_id_for_team_ptr(team);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_league_id == 0u
            || team_id == 0u
            || player_id == 0u
            || kbo_amateur_assignment_already_processed(player_id, team_id)) {
        return;
    }
    kbo_amateur_assignment_mark_processed(player_id, team_id);
    kbo_amateur_assignment_note_player_count_delta(team_league_id, team_id, player, 1);

    if (!read_kbo_localappdata_flag_file("enable_amateur_assignment_debug_csv.txt")) {
        return;
    }

    uint8_t reputation = 0u;
    int team_tier = -1;
    int32_t player_count = -1;
    kbo_amateur_assignment_find_candidate_info(
        team_league_id,
        team_id,
        &reputation,
        &team_tier,
        &player_count,
        NULL);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    int32_t quality_score = kbo_amateur_quality_score(player);
    int player_tier = kbo_amateur_assignment_player_tier(team_league_id, quality_score);
    uint32_t after_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    kbo_amateur_assignment_append_debug_csv(
        "original_success",
        source,
        player_id,
        team_league_id,
        age,
        quality_score,
        player_tier,
        team_id,
        reputation,
        team_tier,
        player_count,
        team_id,
        reputation,
        team_tier,
        player_count,
        player_count,
        reputation,
        original_result,
        after_team_id,
        after_league_id);
}

static int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age)
{
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return age >= 17 && age <= 25;
    }
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return age >= 14 && age <= 18;
    }
    return 0;
}
