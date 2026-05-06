/* Team assignment (from team_continuity) */

typedef uint8_t (__fastcall *OotpTeamAddPlayerFn)(void* team, void* player);
typedef void    (__fastcall *OotpTeamRemovePlayerIdFn)(void* team, uint32_t player_id, uint8_t r2760, uint8_t r2da0, uint8_t r3700);

static OotpTeamAddPlayerFn get_ootp_team_add_player_fn(void)
{
    static int logged = 0;
    if (!logged) {
        logged = 1;
        append_log_line("team add player RVA removed; using fallback array update path.");
    }
    return NULL;
}

static OotpTeamRemovePlayerIdFn get_ootp_team_remove_player_id_fn(void)
{
    static int logged = 0;
    if (!logged) {
        logged = 1;
        append_log_line("team remove player RVA removed; using fallback array update path.");
    }
    return NULL;
}

static void kbo_assign_player_to_team_internal(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int update_organization,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach)
{
    if (out_called_pre_change != NULL) { *out_called_pre_change = 0; }
    if (out_called_register   != NULL) { *out_called_register   = 0; }
    if (out_called_attach     != NULL) { *out_called_attach     = 0; }

    if (player == NULL || team == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(team, OOTP27_KBO_TEAM_ID_OFFSET + sizeof(uint32_t))) {
        return;
    }

    uint32_t to_team_numeric_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (to_team_numeric_id == 0) {
        return;
    }

    uint32_t player_id                  = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id            = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t current_league_id          = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t original_organization_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == 0) {
        team_league_id = fallback_league_id;
    }

    uint32_t parent_team_id = 0;
    if (memory_range_readable(team + 0x130, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + 0x130);
    }

    uint8_t* old_team = NULL;
    if (current_team_id != 0 && current_team_id != to_team_numeric_id) {
        old_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
    }

    OotpTeamRemovePlayerIdFn remove_player_id = get_ootp_team_remove_player_id_fn();
    if (old_team != NULL && player_id != 0) {
        if (remove_player_id != NULL) {
            remove_player_id(old_team, player_id, 1, 1, 1);
            if (out_called_pre_change != NULL) { *out_called_pre_change = 1; }
        } else if (kbo_remove_player_id_from_known_team_roster_arrays(old_team, player_id) > 0
                && out_called_pre_change != NULL) {
            *out_called_pre_change = 1;
        }
    }

    uint8_t added_by_ootp = 0;
    OotpTeamAddPlayerFn add_player = get_ootp_team_add_player_fn();
    if (add_player != NULL) {
        added_by_ootp = add_player(team, player);
        if (out_called_register != NULL) { *out_called_register = 1; }
        if (added_by_ootp && out_called_attach != NULL) { *out_called_attach = 1; }
    }

    if (!added_by_ootp) {
        if (player_id != 0) {
            kbo_add_player_id_to_team_assignment_arrays(team, player_id);
        }
        if (current_team_id != 0 && *(uint32_t*)(player + 0x5c) != current_team_id) {
            *(uint32_t*)(player + 0x5c) = current_team_id;
        }
        if (current_league_id != 0 && *(uint32_t*)(player + 0x4c) != current_league_id) {
            *(uint32_t*)(player + 0x4c) = current_league_id;
        }
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)   = to_team_numeric_id;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = team_league_id;
        *(uint16_t*)(player + 0x41) = 0x0100;
    }

    if (update_organization) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) =
            parent_team_id != 0 ? parent_team_id : to_team_numeric_id;
    } else {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_organization_team;
    }
    *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = fallback_league_id;
}

static void kbo_assign_player_to_team_like_ootp(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach)
{
    kbo_assign_player_to_team_internal(
        player, team, fallback_league_id, 1,
        out_called_pre_change, out_called_register, out_called_attach);
}
