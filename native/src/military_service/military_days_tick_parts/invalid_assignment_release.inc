static int kbo_release_invalid_military_service_team_assignment(
    uint8_t* player,
    uint8_t* service_team,
    uint32_t service_team_id,
    const char* source,
    uint32_t vector_offset)
{
    if (player == NULL
            || service_team == NULL
            || service_team_id == 0u
            || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return 0;
    }

    uint32_t old_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t old_current_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t old_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t old_original_slot_team = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint8_t old_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    int32_t old_contract_status = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
    int32_t old_salary_y1 = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t old_military_days_left = kbo_military_days_left(player);
    uint8_t old_military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
    int removed = kbo_remove_player_id_from_known_team_roster_arrays(service_team, player_id);

    uint32_t fallback_league_id = kbo_resolve_kbo_league_id();
    if (fallback_league_id == 0u) {
        fallback_league_id = old_current_league;
    }

    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = fallback_league_id;
    if (old_active_team == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (old_original_slot_team == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0u;
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0u;
        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0;
        player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] = 1;
    }

    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET) = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 0u;
    memset(
        player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
        0,
        OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t));
    clear_kbo_military_status_flags(player);
    kbo_set_military_days_left(player, 0);

    static volatile LONG invalid_release_log_count = 0;
    LONG slot = InterlockedIncrement(&invalid_release_log_count);
    if (slot <= 160) {
        append_logf(
            "KBO military invalid FA assignment released source=%s player=%u service_team=%u old_current_team=%u old_current_league=%u new_league=%u old_active_team=%u old_original_slot=%u removed=%d old_contract_level=%u old_contract_status=%d old_salary_y1=%d old_days_left=%d old_military_active=%u vector_off=0x%x",
            source != NULL ? source : "",
            player_id,
            service_team_id,
            old_current_team,
            old_current_league,
            fallback_league_id,
            old_active_team,
            old_original_slot_team,
            removed,
            (unsigned)old_contract_level,
            old_contract_status,
            old_salary_y1,
            old_military_days_left,
            (unsigned)old_military_active,
            vector_offset);
    }
    return 1;
}
