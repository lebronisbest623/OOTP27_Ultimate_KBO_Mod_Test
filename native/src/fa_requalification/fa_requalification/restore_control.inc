static int kbo_restore_fa_requalification_team_control(
    const KboFaRequalificationRecord* rec,
    uint32_t current_year,
    const char* source)
{
    if (rec == NULL || rec->player_id == 0u || rec->original_team_id == 0u
            || rec->last_fa_year < 1982u) {
        return 0;
    }
    if (current_year >= rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=already_eligible current_year=%u last_fa_year=%u eligible_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year,
                rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS);
        }
        return 0;
    }

    uint8_t* player = kbo_find_fa_requalification_player_by_id(rec->player_id);
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->original_team_id, 1);
    if (player == NULL) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=player_not_found current_year=%u last_fa_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year);
        }
        return 0;
    }
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=team_not_found current_year=%u last_fa_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year);
        }
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t old_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t old_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t old_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int changed = 0;

    if (old_current_team != rec->original_team_id) {
        if (old_current_team != 0u) {
            uint8_t* old_team = find_kbo_team_by_numeric_id_any_league(old_current_team, 1);
            if (old_team != NULL) {
                kbo_remove_player_id_from_known_team_roster_arrays(old_team, rec->player_id);
            }
        }
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = rec->original_team_id;
        changed = 1;
    }
    if (old_active_team != rec->original_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = rec->original_team_id;
        changed = 1;
    }
    if (league_id != 0u && old_league != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (league_id != 0u && *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    kbo_add_player_id_to_team_assignment_arrays(team, rec->player_id);

    if (changed) {
        append_logf(
            "KBO FA requalification restored team control source=%s player=%u team=%u year=%u last_fa_year=%u eligible_year=%u old_team=%u old_active=%u old_league=%u league=%u",
            source != NULL ? source : "",
            rec->player_id,
            rec->original_team_id,
            current_year,
            rec->last_fa_year,
            rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS,
            old_current_team,
            old_active_team,
            old_league,
            league_id);
    } else {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification checked source=%s player=%u team=%u reason=already_controlled current_year=%u last_fa_year=%u eligible_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year,
                rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS);
        }
    }
    return changed;
}
