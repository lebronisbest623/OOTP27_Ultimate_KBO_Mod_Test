static uint8_t* kbo_find_fa_requalification_player_by_id(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}

static int kbo_fa_requalification_team_ptr_is_kbo(
    uintptr_t team_ptr,
    uint32_t* out_team_id,
    uint32_t* out_league_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (out_team_id != NULL) {
        *out_team_id = team_id;
    }
    if (out_league_id != NULL) {
        *out_league_id = league_id;
    }
    if (team_id == 0u || team_id > 100000u || league_id == 0u || league_id > 100000u) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    return kbo_league_id == 0u || league_id == kbo_league_id;
}

__declspec(noinline) int ootp_kbo_fa_signing_branch_wrapper(uintptr_t player_ptr, uintptr_t team_ptr)
{
    if (!kbo_fix_enabled()) {
        return 1;
    }
    if (!kbo_player_pointer_plausible(player_ptr)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=bad_player player_ptr=%p team_ptr=%p", (void*)player_ptr, (void*)team_ptr);
        }
        return 1;
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    if (!kbo_fa_requalification_team_ptr_is_kbo(team_ptr, &team_id, &league_id)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=non_kbo_team player_ptr=%p team_ptr=%p team=%u league=%u", (void*)player_ptr, (void*)team_ptr, team_id, league_id);
        }
        return 1;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u || player_id > 1000000u) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=bad_player_id player=%u team=%u league=%u", player_id, team_id, league_id);
        }
        return 1;
    }

    if (kbo_custom_foreign_policy_enabled()
            && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = 0u;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_final_signing(
            team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            uint32_t today = 0u;
            if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
                kbo_get_current_yyyymmdd(&today);
            }
            kbo_record_recent_custom_foreign_policy_block(player_id, team_id, today);
            static volatile LONG final_block_log_count = 0;
            LONG slot = InterlockedIncrement(&final_block_log_count);
            if (slot <= 200) {
                append_logf(
                    "custom foreign policy FA signing blocked player=%u team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    player_id,
                    team_id,
                    effective_before,
                    effective_after,
                    effective_limit,
                    kbo_foreign_injury_slot_label(slot_type),
                    injured_player_id,
                    today);
            }
            return 0;
        }
    }

    if (!read_kbo_localappdata_flag_file("enable_fa_requalification.txt")) {
        return 1;
    }

    uint32_t signing_year = kbo_find_league_year_from_id(league_id);
    if (signing_year < 1982u || signing_year > 2200u) {
        uint32_t today = 0u;
        if (kbo_get_current_yyyymmdd(&today)) {
            signing_year = today / 10000u;
        }
    }
    if (signing_year < 1982u || signing_year > 2200u) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=no_signing_year player=%u team=%u league=%u", player_id, team_id, league_id);
        }
        return 1;
    }

    kbo_record_fa_requalification_signing(player_id, team_id, signing_year, "fa_signing_branch");
    return 1;
}
