static int kbo_find_amateur_team_reputation_by_memory_team(uint32_t league_id, uint8_t* team, uint8_t* out_reputation)
{
    if (out_reputation != NULL) {
        *out_reputation = 70u;
    }
    if (league_id == 0u || team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_amateur_resolved_team_reputation_count; i++) {
        if (g_kbo_amateur_resolved_team_reputations[i].team == team
                && g_kbo_amateur_resolved_team_reputations[i].league_id == league_id) {
            if (out_reputation != NULL) {
                *out_reputation = g_kbo_amateur_resolved_team_reputations[i].reputation;
            }
            return 1;
        }
    }
    uint32_t numeric_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (kbo_find_amateur_team_reputation_for_league(league_id, numeric_team_id, out_reputation)) {
        if (g_kbo_amateur_resolved_team_reputation_count < KBO_AMATEUR_ASSIGNMENT_TEAM_MAX) {
            KboAmateurResolvedTeamReputation* resolved = &g_kbo_amateur_resolved_team_reputations[g_kbo_amateur_resolved_team_reputation_count++];
            resolved->team = team;
            resolved->league_id = league_id;
            resolved->reputation = out_reputation != NULL ? *out_reputation : 70u;
        }
        return 1;
    }

    kbo_ensure_amateur_reputation_seeds_loaded();
    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        KboAmateurReputationSeed* seed = &g_kbo_amateur_reputation_seeds[i];
        if (seed->league_id != league_id) {
            continue;
        }
        if ((seed->team_abbr[0] != '\0' && team_has_ootp_string_text(team, seed->team_abbr))
                || (seed->team_name[0] != '\0' && team_has_ootp_string_text(team, seed->team_name))) {
            if (out_reputation != NULL) {
                *out_reputation = seed->reputation;
            }
            if (g_kbo_amateur_resolved_team_reputation_count < KBO_AMATEUR_ASSIGNMENT_TEAM_MAX) {
                KboAmateurResolvedTeamReputation* resolved = &g_kbo_amateur_resolved_team_reputations[g_kbo_amateur_resolved_team_reputation_count++];
                resolved->team = team;
                resolved->league_id = league_id;
                resolved->reputation = seed->reputation;
            }
            kbo_unlock_amateur_reputation_seeds();
            return 1;
        }
    }
    kbo_unlock_amateur_reputation_seeds();
    return 0;
}

static uint32_t kbo_resolve_amateur_assignment_league_id_for_team_ptr(uint8_t* team)
{
    if (team == NULL) {
        return 0u;
    }
    uint8_t reputation = 0u;
    if (kbo_find_amateur_team_reputation_by_memory_team(KBO_HIGH_SCHOOL_LEAGUE_ID, team, &reputation)) {
        return KBO_HIGH_SCHOOL_LEAGUE_ID;
    }
    if (kbo_find_amateur_team_reputation_by_memory_team(KBO_COLLEGE_LEAGUE_ID, team, &reputation)) {
        return KBO_COLLEGE_LEAGUE_ID;
    }
    return 0u;
}
