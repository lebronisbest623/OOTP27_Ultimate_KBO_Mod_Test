/* Foreign reserve-right retention mutation. Included from native/src/foreign_waiver_ai.inc. */

static int kbo_retain_foreign_player_rights(
    uint8_t* player,
    uint8_t* retaining_team,
    uint32_t fallback_league_id,
    uint32_t player_id,
    uint32_t team_id)
{
    if (player == NULL || retaining_team == NULL || player_id == 0 || team_id == 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(retaining_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t team_numeric_id = *(uint32_t*)(retaining_team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (team_numeric_id != team_id) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(retaining_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == 0) {
        team_league_id = fallback_league_id;
    }
    if (team_league_id == 0) {
        return 0;
    }

    uint32_t today_yyyymmdd = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today_yyyymmdd)) {
        return 0;
    }
    uint32_t expires_yyyymmdd = kbo_add_years_yyyymmdd(today_yyyymmdd, KBO_FOREIGN_WAIVER_RETENTION_YEARS);
    if (expires_yyyymmdd == 0u) {
        return 0;
    }

    if (!kbo_set_foreign_waiver_right(team_id, player_id, team_league_id, today_yyyymmdd, expires_yyyymmdd)) {
        append_logf("foreign reserve rights: failed to store right team=%u player=%u", team_id, player_id);
        return 0;
    }

    if (!kbo_add_player_id_to_team_fixed_array(retaining_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)) {
        append_logf("foreign reserve rights: restricted array full team=%u player=%u", team_id, player_id);
        return 0;
    }

    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1;
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = team_id;
    *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = team_league_id;

    append_logf("foreign reserve rights: retained team=%u player=%u league=%u from=%u until=%u",
               team_id, player_id, team_league_id, today_yyyymmdd, expires_yyyymmdd);
    return 1;
}
