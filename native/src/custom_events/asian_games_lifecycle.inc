/* Asian Games replacement, departure, and return lifecycle. Included from native/src/custom_events.inc. */

static int kbo_asian_games_roster_contains_player(uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].player_id == player_id) {
            return 1;
        }
    }
    return 0;
}

static int kbo_asian_games_roster_wildcard_count_except(LONG except_index)
{
    int count = 0;
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (i != except_index && g_kbo_asian_games_roster[i].wildcard != 0u) {
            count++;
        }
    }
    return count;
}

static int kbo_asian_games_replacement_allowed_for_org(uint32_t new_org_id, uint32_t old_org_id, LONG old_index)
{
    if (new_org_id == 0u || new_org_id == old_org_id) {
        return 1;
    }

    int count = 0;
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (i == old_index) {
            continue;
        }
        uint32_t roster_org_id = kbo_asian_games_org_team_id_for_team(g_kbo_asian_games_roster[i].original_team_id);
        if (roster_org_id == new_org_id) {
            count++;
        }
    }
    return count < KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS;
}

static int kbo_asian_games_player_unavailable_for_departure(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 1;
    }
    return player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u
        || player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
        || player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] != 0u;
}

static int kbo_asian_games_find_replacement_for_entry(
    KboAsianGamesRosterEntry* old_entry,
    LONG old_index,
    KboAsianGamesRosterEntry* out_entry,
    uint32_t* out_org_id,
    const char* source)
{
    if (old_entry == NULL || out_entry == NULL) {
        return 0;
    }
    memset(out_entry, 0, sizeof(*out_entry));
    if (out_org_id != NULL) {
        *out_org_id = 0u;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        append_logf("KBO Asian Games replacement skipped source=%s old_player=%u reason=player_vector_unavailable", source != NULL ? source : "", old_entry->player_id);
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
    uint32_t allowed_leagues[KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES] = {0};
    int allowed_league_count = 0;
    if (kbo_league_id != 0u) {
        allowed_leagues[allowed_league_count++] = kbo_league_id;
    }
    if (kbo_league_id == 200u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = 201u;
    }

    int wildcard_count_without_old = kbo_asian_games_roster_wildcard_count_except(old_index);
    uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(old_entry->original_team_id);
    int best_score = -2147483647;
    KboAsianGamesRosterEntry best_entry;
    memset(&best_entry, 0, sizeof(best_entry));
    uint32_t best_org_id = 0u;
    int scanned = 0;
    int rejected_service_team = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || player_id == old_entry->player_id || kbo_asian_games_roster_contains_player(player_id)) {
            continue;
        }
        if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (kbo_asian_games_player_unavailable_for_departure(player)) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (!kbo_asian_games_league_allowed(current_league_id, allowed_leagues, allowed_league_count)) {
            continue;
        }
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        if (current_team == NULL
                || *(uint32_t*)(current_team + OOTP27_KBO_TEAM_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(current_team)) {
            rejected_service_team++;
            continue;
        }

        uint8_t role = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
        if (!kbo_asian_games_roles_same_bucket(role, old_entry->role)) {
            continue;
        }
        uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        int wildcard = age > 24u;
        if (wildcard && wildcard_count_without_old >= KBO_ASIAN_GAMES_MAX_WILDCARDS) {
            continue;
        }
        uint32_t org_id = kbo_asian_games_org_team_id_for_team(current_team_id);
        if (!kbo_asian_games_replacement_allowed_for_org(org_id, old_org_id, old_index)) {
            continue;
        }

        int score = kbo_asian_games_player_score(player);
        if (best_entry.player_id == 0u || score > best_score) {
            memset(&best_entry, 0, sizeof(best_entry));
            best_entry.player_id = player_id;
            best_entry.original_team_id = current_team_id;
            best_entry.original_league_id = current_league_id;
            best_entry.age = age;
            best_entry.role = role;
            best_entry.wildcard = wildcard ? 1u : 0u;
            best_entry.score = score;
            best_entry.player_ptr = player_ptr;
            best_score = score;
            best_org_id = org_id;
        }
    }

    if (best_entry.player_id == 0u) {
        append_logf(
            "KBO Asian Games replacement unavailable source=%s old_player=%u old_role=%u scanned=%d rejected_service_team=%d vector_offset=0x%x",
            source != NULL ? source : "",
            old_entry->player_id,
            (uint32_t)old_entry->role,
            scanned,
            rejected_service_team,
            vector_offset);
        return 0;
    }

    *out_entry = best_entry;
    if (out_org_id != NULL) {
        *out_org_id = best_org_id;
    }
    return 1;
}

static int kbo_emit_asian_games_replacement_news(
    uint32_t event_yyyymmdd,
    KboAsianGamesRosterEntry* old_entry,
    KboAsianGamesRosterEntry* new_entry,
    const char* source);

static int kbo_asian_games_replace_unavailable_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    int replacements = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->departed != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player != NULL && kbo_player_pointer_plausible((uintptr_t)player)) {
            entry->player_ptr = (uintptr_t)player;
        }
        if (!kbo_asian_games_player_unavailable_for_departure(player)) {
            continue;
        }

        KboAsianGamesRosterEntry old_entry = *entry;
        KboAsianGamesRosterEntry new_entry;
        uint32_t new_org_id = 0u;
        if (!kbo_asian_games_find_replacement_for_entry(entry, i, &new_entry, &new_org_id, source)) {
            append_logf(
                "KBO Asian Games replacement failed source=%s date=%u old_player=%u slot=%ld",
                source != NULL ? source : "",
                event_yyyymmdd,
                entry->player_id,
                i + 1);
            continue;
        }

        g_kbo_asian_games_roster[i] = new_entry;
        replacements++;
        append_logf(
            "KBO Asian Games injury replacement source=%s date=%u slot=%ld old_player=%u old_team=%u new_player=%u new_team=%u new_org=%u wildcard=%u score=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            i + 1,
            old_entry.player_id,
            old_entry.original_team_id,
            new_entry.player_id,
            new_entry.original_team_id,
            new_org_id,
            (uint32_t)new_entry.wildcard,
            new_entry.score);
        kbo_emit_asian_games_replacement_news(event_yyyymmdd, &old_entry, &new_entry, source);
    }

    if (replacements > 0) {
        kbo_save_asian_games_roster_csv(source);
    }
    return replacements;
}

static void kbo_record_asian_games_restricted_reason(
    KboAsianGamesRosterEntry* entry,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    uint32_t team_id,
    const char* source)
{
    if (entry == NULL || entry->player_id == 0u || event_yyyymmdd == 0u || league_id == 0u || team_id == 0u) {
        return;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;

    char player_link[128] = {0};
    char team_link[128] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));
    kbo_copy_asian_games_team_link(team_id, team_link, sizeof(team_link));

    char league_text[512] = {0};
    char team_text[512] = {0};
    char history_text[512] = {0};
    snprintf(
        league_text,
        sizeof(league_text),
        "%s: Placed %s on the restricted list for Asian Games national team duty.",
        team_link[0] != '\0' ? team_link : "KBO club",
        player_link[0] != '\0' ? player_link : "a player");
    snprintf(
        team_text,
        sizeof(team_text),
        "Placed %s on the restricted list for Asian Games national team duty.",
        player_link[0] != '\0' ? player_link : "a player");
    snprintf(
        history_text,
        sizeof(history_text),
        "Placed on the restricted list for Asian Games national team duty.");

    insert_kbo_roster_transaction_sql(
        league_id,
        team_id,
        year,
        month,
        day,
        0u,
        league_text,
        team_text,
        source);
    insert_kbo_player_history_sql(
        entry->player_id,
        year,
        month,
        day,
        history_text,
        source);
}

static int kbo_asian_games_depart_selected_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        append_logf(
            "KBO Asian Games departure skipped source=%s date=%u reason=no_roster count=%ld year=%u",
            source != NULL ? source : "",
            event_yyyymmdd,
            roster_count,
            g_kbo_asian_games_roster_year);
        return 0;
    }

    int replacements = kbo_asian_games_replace_unavailable_players(event_yyyymmdd, source);
    if (replacements > 0) {
        roster_count = g_kbo_asian_games_roster_count;
    }

    int departed = 0;
    int missing = 0;
    int no_team = 0;
    int32_t days_left = kbo_asian_games_days_until_return(event_yyyymmdd);
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->departed != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            missing++;
            append_logf(
                "KBO Asian Games departure missing player_id=%u index=%ld",
                entry->player_id,
                i + 1);
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        if (team == NULL) {
            no_team++;
            append_logf(
                "KBO Asian Games departure no team player_id=%u team=%u league=%u index=%ld",
                entry->player_id,
                current_team_id,
                current_league_id,
                i + 1);
            continue;
        }

        entry->player_ptr = (uintptr_t)player;
        if (entry->original_team_id == 0u) {
            entry->original_team_id = current_team_id;
        }
        if (entry->original_league_id == 0u) {
            entry->original_league_id = current_league_id;
        }
        entry->old_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        entry->old_secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        entry->old_injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        entry->old_injury_days_left = (int16_t)kbo_military_days_left(player);
        entry->departure_date = event_yyyymmdd;
        entry->return_date = kbo_asian_games_final_date_for_year(event_yyyymmdd / 10000u);

        int removed = kbo_remove_player_id_from_known_team_roster_arrays(team, entry->player_id);
        int added_restricted = kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, entry->player_id);
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] = 0u;
        kbo_set_military_days_left(player, days_left);
        kbo_record_asian_games_restricted_reason(
            entry,
            event_yyyymmdd,
            current_league_id,
            current_team_id,
            source);
        entry->departed = 1u;
        departed++;

        append_logf(
            "KBO Asian Games departed #%ld player_id=%u team=%u league=%u role=%u bucket=%s days_left=%d removed=%d added_restricted=%d old_restricted=%u old_secondary=%u old_injury=%u old_days=%d",
            i + 1,
            entry->player_id,
            current_team_id,
            current_league_id,
            (uint32_t)entry->role,
            kbo_asian_games_role_bucket_label(entry->role),
            days_left,
            removed,
            added_restricted,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left);
    }

    append_logf(
        "KBO Asian Games departure processed source=%s date=%u roster=%ld departed=%d missing=%d no_team=%d days_left=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        roster_count,
        departed,
        missing,
        no_team,
        days_left);
    kbo_save_asian_games_roster_csv(source);
    return departed;
}

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


