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

__declspec(noinline) void ootp_kbo_fa_signing_branch_wrapper(uintptr_t player_ptr, uintptr_t team_ptr)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (!kbo_player_pointer_plausible(player_ptr)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=bad_player player_ptr=%p team_ptr=%p", (void*)player_ptr, (void*)team_ptr);
        }
        return;
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    if (!kbo_fa_requalification_team_ptr_is_kbo(team_ptr, &team_id, &league_id)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=non_kbo_team player_ptr=%p team_ptr=%p team=%u league=%u", (void*)player_ptr, (void*)team_ptr, team_id, league_id);
        }
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u || player_id > 1000000u) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_hook_skip_log_count);
        if (slot <= 20) {
            append_logf("KBO FA signing branch skipped reason=bad_player_id player=%u team=%u league=%u", player_id, team_id, league_id);
        }
        return;
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
        return;
    }

    kbo_record_fa_requalification_signing(player_id, team_id, signing_year, "fa_signing_branch");
}

