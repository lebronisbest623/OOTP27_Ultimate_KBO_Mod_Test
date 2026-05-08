/* Asian Games roster selection. Included from native/src/custom_events.inc. */

static int kbo_asian_games_try_select_candidate(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int index,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count,
    int enforce_team_max)
{
    if (candidates == NULL || index < 0 || index >= candidate_count
            || selected_count == NULL || pitcher_count == NULL
            || catcher_count == NULL || infielder_count == NULL
            || outfielder_count == NULL || wildcard_count == NULL) {
        return 0;
    }
    if (*selected_count >= KBO_ASIAN_GAMES_ROSTER_SIZE || candidates[index].selected) {
        return 0;
    }

    KboAsianGamesRosterEntry* entry = &candidates[index].entry;
    if (kbo_asian_games_role_is_pitcher(entry->role)) {
        if (*pitcher_count >= KBO_ASIAN_GAMES_PITCHER_TARGET) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_catcher(entry->role)) {
        if (*catcher_count >= KBO_ASIAN_GAMES_CATCHER_TARGET) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_infielder(entry->role)) {
        if (*infielder_count >= KBO_ASIAN_GAMES_INFIELDER_TARGET) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_outfielder(entry->role)) {
        if (*outfielder_count >= KBO_ASIAN_GAMES_OUTFIELDER_TARGET) {
            return 0;
        }
    } else {
        return 0;
    }

    int wildcard = entry->age > 24u;
    if (wildcard && *wildcard_count >= KBO_ASIAN_GAMES_MAX_WILDCARDS) {
        return 0;
    }
    if (enforce_team_max && candidates[index].org_team_id != 0u
            && kbo_asian_games_roster_org_count(candidates[index].org_team_id, *selected_count) >= KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS) {
        return 0;
    }

    entry->wildcard = wildcard ? 1u : 0u;
    g_kbo_asian_games_roster[*selected_count] = *entry;
    candidates[index].selected = 1u;
    (*selected_count)++;
    if (kbo_asian_games_role_is_pitcher(entry->role)) {
        (*pitcher_count)++;
    } else if (kbo_asian_games_role_is_catcher(entry->role)) {
        (*catcher_count)++;
    } else if (kbo_asian_games_role_is_infielder(entry->role)) {
        (*infielder_count)++;
    } else {
        (*outfielder_count)++;
    }
    if (wildcard) {
        (*wildcard_count)++;
    }
    return 1;
}

static int kbo_asian_games_try_select_candidate_flex_position(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int index,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count,
    int enforce_team_max)
{
    if (candidates == NULL || index < 0 || index >= candidate_count
            || selected_count == NULL || pitcher_count == NULL || catcher_count == NULL
            || infielder_count == NULL || outfielder_count == NULL || wildcard_count == NULL) {
        return 0;
    }
    if (*selected_count >= KBO_ASIAN_GAMES_ROSTER_SIZE || candidates[index].selected) {
        return 0;
    }

    KboAsianGamesRosterEntry* entry = &candidates[index].entry;
    if (!kbo_asian_games_role_is_pitcher(entry->role)
            && !kbo_asian_games_role_is_catcher(entry->role)
            && !kbo_asian_games_role_is_infielder(entry->role)
            && !kbo_asian_games_role_is_outfielder(entry->role)) {
        return 0;
    }

    int wildcard = entry->age > 24u;
    if (wildcard && *wildcard_count >= KBO_ASIAN_GAMES_MAX_WILDCARDS) {
        return 0;
    }
    if (enforce_team_max && candidates[index].org_team_id != 0u
            && kbo_asian_games_roster_org_count(candidates[index].org_team_id, *selected_count) >= KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS) {
        return 0;
    }

    entry->wildcard = wildcard ? 1u : 0u;
    g_kbo_asian_games_roster[*selected_count] = *entry;
    candidates[index].selected = 1u;
    (*selected_count)++;
    if (kbo_asian_games_role_is_pitcher(entry->role)) {
        (*pitcher_count)++;
    } else if (kbo_asian_games_role_is_catcher(entry->role)) {
        (*catcher_count)++;
    } else if (kbo_asian_games_role_is_infielder(entry->role)) {
        (*infielder_count)++;
    } else {
        (*outfielder_count)++;
    }
    if (wildcard) {
        (*wildcard_count)++;
    }
    return 1;
}
