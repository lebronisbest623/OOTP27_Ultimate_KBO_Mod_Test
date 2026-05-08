/* Team assignment (from team_continuity) */

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
    if (memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    }

    uint8_t* old_team = NULL;
    if (current_team_id != 0 && current_team_id != to_team_numeric_id) {
        old_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
    }

    if (old_team != NULL && player_id != 0) {
        if (kbo_remove_player_id_from_known_team_roster_arrays(old_team, player_id) > 0
                && out_called_pre_change != NULL) {
            *out_called_pre_change = 1;
        }
    }

    if (player_id != 0) {
        kbo_add_player_id_to_team_assignment_arrays(team, player_id);
    }
    if (current_team_id != 0 && *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) != current_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = current_team_id;
    }
    if (current_league_id != 0 && *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) != current_league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = current_league_id;
    }
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)   = to_team_numeric_id;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = team_league_id;
    *(uint16_t*)(player + OOTP27_PLAYER_STATUS_FLAGS_OFFSET) = 0x0100;

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
