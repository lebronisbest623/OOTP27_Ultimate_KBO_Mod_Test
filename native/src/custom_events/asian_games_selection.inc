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

static int kbo_asian_games_apply_wildcard_replacements(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int selected_count,
    int* wildcard_count,
    const uint32_t* required_orgs,
    int required_org_count)
{
    if (candidates == NULL || candidate_count <= 0 || selected_count <= 0 || wildcard_count == NULL) {
        return 0;
    }

    int replacements = 0;
    while (*wildcard_count < KBO_ASIAN_GAMES_MAX_WILDCARDS) {
        int best_candidate_index = -1;
        int best_roster_index = -1;
        int best_gain = -2147483647;
        int best_was_military_ready = 0;

        for (int i = 0; i < candidate_count; i++) {
            KboAsianGamesCandidate* candidate = &candidates[i];
            if (candidate->selected || candidate->entry.age <= 24u) {
                continue;
            }
            int candidate_military_ready = 1;
            uintptr_t player_ptr = candidate->entry.player_ptr;
            if (kbo_player_pointer_plausible(player_ptr)) {
                uint8_t* player = (uint8_t*)player_ptr;
                candidate_military_ready = player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] == 0u
                    && player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u;
            }

            for (int roster_index = 0; roster_index < selected_count; roster_index++) {
                KboAsianGamesRosterEntry* current = &g_kbo_asian_games_roster[roster_index];
                if (current->wildcard != 0u || current->age > 24u) {
                    continue;
                }
                if (!kbo_asian_games_roles_same_bucket(candidate->entry.role, current->role)) {
                    continue;
                }

                uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(current->original_team_id);
                if (candidate->org_team_id != 0u
                        && old_org_id != candidate->org_team_id
                        && kbo_asian_games_roster_org_count(candidate->org_team_id, selected_count) >= KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS) {
                    continue;
                }
                int old_org_required = kbo_asian_games_find_org_index(required_orgs, required_org_count, old_org_id) >= 0;
                if (old_org_required
                        && old_org_id != candidate->org_team_id
                        && kbo_asian_games_roster_org_count(old_org_id, selected_count) <= KBO_ASIAN_GAMES_TEAM_MIN_PLAYERS) {
                    continue;
                }

                int gain = candidate->entry.score - current->score;
                if (best_candidate_index < 0
                        || (candidate_military_ready && !best_was_military_ready)
                        || (candidate_military_ready == best_was_military_ready && gain > best_gain)) {
                    best_candidate_index = i;
                    best_roster_index = roster_index;
                    best_gain = gain;
                    best_was_military_ready = candidate_military_ready;
                }
            }
        }

        if (best_candidate_index < 0 || best_roster_index < 0) {
            break;
        }

        KboAsianGamesRosterEntry old_entry = g_kbo_asian_games_roster[best_roster_index];
        KboAsianGamesCandidate* replacement = &candidates[best_candidate_index];
        replacement->entry.wildcard = 1u;
        g_kbo_asian_games_roster[best_roster_index] = replacement->entry;
        replacement->selected = 1u;
        (*wildcard_count)++;
        replacements++;

        append_logf(
            "KBO Asian Games wildcard replacement slot=%d old_player=%u old_age=%u old_role=%u old_score=%d new_player=%u new_age=%u new_role=%u new_score=%d gain=%d military_ready=%d",
            best_roster_index + 1,
            old_entry.player_id,
            (uint32_t)old_entry.age,
            (uint32_t)old_entry.role,
            old_entry.score,
            replacement->entry.player_id,
            (uint32_t)replacement->entry.age,
            (uint32_t)replacement->entry.role,
            replacement->entry.score,
            best_gain,
            best_was_military_ready);
    }

    return replacements;
}

static int kbo_asian_games_replace_for_missing_org(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    uint32_t missing_org_id,
    int selected_count,
    const uint32_t* required_orgs,
    int required_org_count)
{
    if (candidates == NULL || candidate_count <= 0 || missing_org_id == 0u || selected_count <= 0) {
        return 0;
    }

    int best_candidate_index = -1;
    int best_roster_index = -1;
    int best_loss = 2147483647;

    for (int i = 0; i < candidate_count; i++) {
        KboAsianGamesCandidate* candidate = &candidates[i];
        if (candidate->selected || candidate->org_team_id != missing_org_id || candidate->entry.age > 24u) {
            continue;
        }

        for (int roster_index = 0; roster_index < selected_count; roster_index++) {
            KboAsianGamesRosterEntry* current = &g_kbo_asian_games_roster[roster_index];
            if (current->wildcard != 0u || current->age > 24u) {
                continue;
            }
            if (!kbo_asian_games_roles_same_bucket(candidate->entry.role, current->role)) {
                continue;
            }

            uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(current->original_team_id);
            if (old_org_id == missing_org_id) {
                continue;
            }
            int old_org_required = kbo_asian_games_find_org_index(required_orgs, required_org_count, old_org_id) >= 0;
            if (old_org_required && kbo_asian_games_roster_org_count(old_org_id, selected_count) <= KBO_ASIAN_GAMES_TEAM_MIN_PLAYERS) {
                continue;
            }

            int loss = current->score - candidate->entry.score;
            if (best_candidate_index < 0 || loss < best_loss) {
                best_candidate_index = i;
                best_roster_index = roster_index;
                best_loss = loss;
            }
        }
    }

    if (best_candidate_index < 0 || best_roster_index < 0) {
        return 0;
    }

    KboAsianGamesRosterEntry old_entry = g_kbo_asian_games_roster[best_roster_index];
    g_kbo_asian_games_roster[best_roster_index] = candidates[best_candidate_index].entry;
    candidates[best_candidate_index].selected = 1u;

    append_logf(
        "KBO Asian Games team-min replacement org=%u slot=%d old_player=%u old_team=%u new_player=%u new_team=%u loss=%d",
        missing_org_id,
        best_roster_index + 1,
        old_entry.player_id,
        old_entry.original_team_id,
        candidates[best_candidate_index].entry.player_id,
        candidates[best_candidate_index].entry.original_team_id,
        best_loss);
    return 1;
}

static int kbo_select_asian_games_roster(uint32_t event_yyyymmdd, const char* source)
{
    uint32_t year = event_yyyymmdd / 10000u;
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        append_logf(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=player_vector_unavailable",
            source != NULL ? source : "",
            year);
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
    uint32_t allowed_leagues[KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES] = {0};
    int allowed_league_count = 0;
    if (kbo_league_id != 0u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = kbo_league_id;
    }
    if (kbo_league_id == 200u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = 201u;
    }

    KboAsianGamesCandidate* candidates = (KboAsianGamesCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(KboAsianGamesCandidate) * KBO_ASIAN_GAMES_MAX_CANDIDATES);
    if (candidates == NULL) {
        append_logf(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=alloc_failed",
            source != NULL ? source : "",
            year);
        return 0;
    }

    int candidate_count = 0;
    int scanned = 0;
    int rejected_nation = 0;
    int rejected_status = 0;
    int rejected_team = 0;
    int rejected_league = 0;
    int rejected_service_team = 0;
    int rejected_parentless_affiliate = 0;
    uint32_t main_league_id = allowed_league_count > 0 ? allowed_leagues[0] : 0u;
    for (int32_t i = 0; i < player_count && candidate_count < KBO_ASIAN_GAMES_MAX_CANDIDATES; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            rejected_nation++;
            continue;
        }
        if (player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] != 0u
                || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
                || player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u
                || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u) {
            rejected_status++;
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (!kbo_asian_games_league_allowed(current_league_id, allowed_leagues, allowed_league_count)) {
            rejected_league++;
            continue;
        }
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        uint32_t team_nation_id = current_team != NULL
            ? *(uint32_t*)(current_team + OOTP27_KBO_TEAM_NATION_ID_OFFSET)
            : 0u;
        if (current_team_id == 0u || current_league_id == 0u
                || current_team == NULL
                || team_nation_id != OOTP27_KBO_KOREA_NATION_ID) {
            rejected_team++;
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(current_team)) {
            rejected_service_team++;
            continue;
        }
        if (!kbo_asian_games_team_has_parent_if_affiliate(current_team, main_league_id)) {
            rejected_parentless_affiliate++;
            continue;
        }

        KboAsianGamesCandidate* candidate = &candidates[candidate_count++];
        candidate->entry.player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        candidate->entry.original_team_id = current_team_id;
        candidate->entry.original_league_id = current_league_id;
        candidate->entry.departure_date = 0u;
        candidate->entry.return_date = 0u;
        candidate->entry.age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        candidate->entry.role = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
        candidate->entry.wildcard = 0u;
        candidate->entry.old_restricted = 0u;
        candidate->entry.old_secondary_restricted = 0u;
        candidate->entry.old_injury_active = 0u;
        candidate->entry.old_injury_days_left = 0;
        candidate->entry.departed = 0u;
        candidate->entry.returned = 0u;
        candidate->entry.exempted = 0u;
        candidate->entry.score = kbo_asian_games_player_score(player);
        candidate->entry.player_ptr = player_ptr;
        candidate->org_team_id = kbo_asian_games_org_team_id_for_team(current_team_id);
    }

    qsort(candidates, (size_t)candidate_count, sizeof(KboAsianGamesCandidate), kbo_asian_games_candidate_compare_desc);
    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    g_kbo_asian_games_roster_count = 0;
    g_kbo_asian_games_roster_year = year;

    uint32_t required_orgs[KBO_ASIAN_GAMES_MAX_ORGS] = {0};
    int required_org_count = kbo_asian_games_collect_required_orgs(
        allowed_league_count > 0 ? allowed_leagues[0] : 0u,
        required_orgs,
        KBO_ASIAN_GAMES_MAX_ORGS);

    int selected_count = 0;
    int pitcher_count = 0;
    int catcher_count = 0;
    int infielder_count = 0;
    int outfielder_count = 0;
    int wildcard_count = 0;

    int required_org_initial_selected = 0;
    for (int org_index = 0; org_index < required_org_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; org_index++) {
        int best_index = -1;
        for (int i = 0; i < candidate_count; i++) {
            if (candidates[i].selected
                    || candidates[i].org_team_id != required_orgs[org_index]
                    || candidates[i].entry.age > 24u) {
                continue;
            }
            best_index = i;
            if (kbo_asian_games_try_select_candidate(
                    candidates,
                    candidate_count,
                    i,
                    &selected_count,
                    &pitcher_count,
                    &catcher_count,
                    &infielder_count,
                    &outfielder_count,
                    &wildcard_count,
                    1)) {
                required_org_initial_selected++;
                break;
            }
        }
        if (best_index >= 0 && kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) == 0) {
            append_logf(
                "KBO Asian Games team-min initial select deferred org=%u best_player=%u best_role=%u reason=position_full_or_team_max",
                required_orgs[org_index],
                candidates[best_index].entry.player_id,
                (uint32_t)candidates[best_index].entry.role);
        }
    }

    for (int i = 0; i < candidate_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        if (candidates[i].entry.age > 24u) {
            continue;
        }
        kbo_asian_games_try_select_candidate(
            candidates,
            candidate_count,
            i,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            1);
    }

    int wildcard_fills = 0;
    for (int i = 0; i < candidate_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        int before_selected = selected_count;
        int before_wildcards = wildcard_count;
        if (candidates[i].entry.age <= 24u) {
            continue;
        }
        kbo_asian_games_try_select_candidate(
            candidates,
            candidate_count,
            i,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            1);
        if (selected_count > before_selected && wildcard_count > before_wildcards) {
            wildcard_fills++;
        }
    }

    int required_org_repairs = 0;
    for (int org_index = 0; org_index < required_org_count; org_index++) {
        if (kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) > 0) {
            continue;
        }
        if (kbo_asian_games_replace_for_missing_org(
                candidates,
                candidate_count,
                required_orgs[org_index],
                selected_count,
                required_orgs,
                required_org_count)) {
            required_org_repairs++;
        }
    }

    int required_org_missing = 0;
    for (int org_index = 0; org_index < required_org_count; org_index++) {
        if (kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) <= 0) {
            required_org_missing++;
        }
    }

    int wildcard_replacements = kbo_asian_games_apply_wildcard_replacements(
        candidates,
        candidate_count,
        selected_count,
        &wildcard_count,
        required_orgs,
        required_org_count);

    g_kbo_asian_games_roster_count = selected_count;
    append_logf(
        "KBO Asian Games roster selected source=%s year=%u scanned=%d candidates=%d selected=%d pitchers=%d catchers=%d infielders=%d outfielders=%d wildcards=%d wildcard_fills=%d wildcard_replacements=%d required_orgs=%d required_initial=%d required_repairs=%d required_missing=%d team_max=%d allowed_leagues=%u/%u vector_offset=0x%x rejected_nation=%d rejected_status=%d rejected_league=%d rejected_team=%d rejected_service_team=%d rejected_parentless_affiliate=%d",
        source != NULL ? source : "",
        year,
        scanned,
        candidate_count,
        selected_count,
        pitcher_count,
        catcher_count,
        infielder_count,
        outfielder_count,
        wildcard_count,
        wildcard_fills,
        wildcard_replacements,
        required_org_count,
        required_org_initial_selected,
        required_org_repairs,
        required_org_missing,
        KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS,
        allowed_league_count > 0 ? allowed_leagues[0] : 0u,
        allowed_league_count > 1 ? allowed_leagues[1] : 0u,
        vector_offset,
        rejected_nation,
        rejected_status,
        rejected_league,
        rejected_team,
        rejected_service_team,
        rejected_parentless_affiliate);
    for (int i = 0; i < selected_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        append_logf(
            "KBO Asian Games selected #%02d player_id=%u team=%u org=%u league=%u age=%u role=%u bucket=%s wildcard=%u score=%d",
            i + 1,
            entry->player_id,
            entry->original_team_id,
            kbo_asian_games_org_team_id_for_team(entry->original_team_id),
            entry->original_league_id,
            (uint32_t)entry->age,
            (uint32_t)entry->role,
            kbo_asian_games_role_bucket_label(entry->role),
            (uint32_t)entry->wildcard,
            entry->score);
    }

    HeapFree(GetProcessHeap(), 0, candidates);
    kbo_save_asian_games_roster_csv(source);
    return selected_count;
}


