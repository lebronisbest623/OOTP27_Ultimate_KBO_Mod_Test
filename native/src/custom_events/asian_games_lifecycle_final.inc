/* Asian Games final return and exemption lifecycle. */

static int kbo_asian_games_finalize_selected_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        append_logf(
            "KBO Asian Games final skipped source=%s date=%u reason=no_roster count=%ld year=%u",
            source != NULL ? source : "",
            event_yyyymmdd,
            roster_count,
            g_kbo_asian_games_roster_year);
        return 0;
    }

    int returned = 0;
    int exempted = 0;
    int missing = 0;
    int no_team = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->returned != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            missing++;
            append_logf(
                "KBO Asian Games final missing player_id=%u index=%ld",
                entry->player_id,
                i + 1);
            continue;
        }

        uint32_t team_id = entry->original_team_id != 0u
            ? entry->original_team_id
            : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t league_id = entry->original_league_id != 0u
            ? entry->original_league_id
            : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
        if (team == NULL) {
            no_team++;
            append_logf(
                "KBO Asian Games final no team player_id=%u team=%u league=%u index=%ld",
                entry->player_id,
                team_id,
                league_id,
                i + 1);
            continue;
        }

        int removed_restricted = kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, entry->player_id);
        int added_assignment = kbo_add_player_id_to_team_assignment_arrays(team, entry->player_id);
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = team_id;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = team_id;

        complete_kbo_military_service_status(player);
        entry->exempted = 1u;
        exempted++;

        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = entry->old_restricted;
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = entry->old_secondary_restricted;
        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] = entry->old_injury_active;
        kbo_set_military_days_left(player, entry->old_injury_days_left);
        if (player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] == 0u) {
            player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] = 1u;
        }

        entry->returned = 1u;
        returned++;
        append_logf(
            "KBO Asian Games finalized #%ld player_id=%u team=%u league=%u exempted=%u removed_restricted=%d added_assignment=%d restored_restricted=%u restored_secondary=%u restored_injury=%u restored_days=%d",
            i + 1,
            entry->player_id,
            team_id,
            league_id,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET],
            removed_restricted,
            added_assignment,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left);
    }

    append_logf(
        "KBO Asian Games final processed source=%s date=%u roster=%ld returned=%d exempted=%d missing=%d no_team=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        roster_count,
        returned,
        exempted,
        missing,
        no_team);
    kbo_save_asian_games_roster_csv(source);
    return returned;
}

static int kbo_asian_games_roster_already_finalized(const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    int finalized = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id != 0u && entry->returned != 0u) {
            finalized++;
        }
    }
    return finalized > 0 && finalized == roster_count;
}
