/* Asian Games news body and event handlers. Included from native/src/custom_events.inc. */

static void kbo_copy_asian_games_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, 0x10u, city, sizeof(city));
        copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));
        if (full_name[0] != '\0' && strchr(full_name, ' ') != NULL) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0' && !ascii_equals_ignore_case(city, nickname)) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (full_name[0] != '\0') {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (nickname[0] != '\0') {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
    }

    if (team_id != 0u) {
        snprintf(out, out_size, "his club");
    }
}

static void kbo_copy_asian_games_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    kbo_copy_asian_games_team_name(team_id, team_name, sizeof(team_name));
    if (team_name[0] == '\0' || ascii_equals_ignore_case(team_name, "his club")) {
        if (team_id != 0u) {
            snprintf(out, out_size, "<Team #%u:team#%u>", team_id, team_id);
        }
        return;
    }
    if (team_id != 0u) {
        snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
    } else {
        snprintf(out, out_size, "%s", team_name);
    }
}

static void kbo_copy_asian_games_player_link(KboAsianGamesRosterEntry* entry, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (entry == NULL || entry->player_id == 0u) {
        snprintf(out, out_size, "a Korea squad member");
        return;
    }

    uintptr_t player_ptr = entry->player_ptr;
    if (!kbo_player_pointer_plausible(player_ptr)) {
        player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
    }

    char player_name[64] = {0};
    if (kbo_player_pointer_plausible(player_ptr)) {
        kbo_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", entry->player_id);
    }

    snprintf(out, out_size, "<%s:player#%u>", player_name, entry->player_id);
}

static int kbo_asian_games_append_player_blurb(
    char* out,
    size_t out_size,
    size_t* used,
    KboAsianGamesRosterEntry* entry,
    LONG display_index,
    LONG display_count)
{
    if (out == NULL || out_size == 0 || used == NULL || *used >= out_size - 1 || entry == NULL
            || display_index < 0 || display_index >= display_count) {
        return 0;
    }

    char player_link[96] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));

    char team_link[128] = {0};
    kbo_copy_asian_games_team_link(entry->original_team_id, team_link, sizeof(team_link));

    const char* bucket = kbo_asian_games_role_bucket_label(entry->role);
    int wrote = snprintf(
        out + *used,
        out_size - *used,
        "%s%s%s%s",
        display_index == 0 ? "" : (display_index == display_count - 1 ? " and " : ", "),
        player_link,
        team_link[0] != '\0' ? " of " : "",
        team_link);
    if (wrote <= 0) {
        return 0;
    }
    *used += (size_t)wrote;
    if (*used < out_size - 1 && bucket[0] != '\0') {
        wrote = snprintf(out + *used, out_size - *used, " (%s)", bucket);
        if (wrote > 0) {
            *used += (size_t)wrote;
        }
    }
    return 1;
}

static int kbo_asian_games_append_roster_line(
    char* out,
    size_t out_size,
    size_t* used,
    LONG index,
    KboAsianGamesRosterEntry* entry)
{
    if (out == NULL || out_size == 0 || used == NULL || *used >= out_size - 1 || entry == NULL) {
        return 0;
    }

    char player_link[96] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));

    char team_link[128] = {0};
    kbo_copy_asian_games_team_link(entry->original_team_id, team_link, sizeof(team_link));
    if (team_link[0] == '\0') {
        snprintf(team_link, sizeof(team_link), "unattached");
    }

    const char* status = "selected";
    if (entry->returned) {
        status = entry->exempted ? "returned, exempt" : "returned";
    } else if (entry->departed) {
        status = "on tournament leave";
    }

    int wrote = snprintf(
        out + *used,
        out_size - *used,
        "%02ld. %s, %s, %s%s, age %u, %s\n",
        index + 1,
        player_link,
        kbo_asian_games_role_bucket_label(entry->role),
        team_link,
        entry->wildcard ? ", wild card" : "",
        (uint32_t)entry->age,
        status);
    if (wrote <= 0) {
        return 0;
    }
    *used += (size_t)wrote;
    return 1;
}

static KboAsianGamesRosterEntry* kbo_asian_games_choose_captain(void)
{
    KboAsianGamesRosterEntry* best = NULL;
    for (LONG i = 0; i < g_kbo_asian_games_roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->wildcard == 0u) {
            continue;
        }
        if (best == NULL || entry->score > best->score) {
            best = entry;
        }
    }
    if (best != NULL) {
        return best;
    }
    for (LONG i = 0; i < g_kbo_asian_games_roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id != 0u && (best == NULL || entry->score > best->score)) {
            best = entry;
        }
    }
    return best;
}

static void kbo_build_asian_games_news_body(char* out, size_t out_size, const char* title, const char* lead)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    int departed = 0;
    int returned = 0;
    int exempted = 0;
    int wildcards = 0;
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].departed) { departed++; }
        if (g_kbo_asian_games_roster[i].returned) { returned++; }
        if (g_kbo_asian_games_roster[i].exempted) { exempted++; }
        if (g_kbo_asian_games_roster[i].wildcard) { wildcards++; }
    }
    KboAsianGamesRosterEntry* captain = kbo_asian_games_choose_captain();
    char captain_link[96] = {0};
    if (captain != NULL) {
        kbo_copy_asian_games_player_link(captain, captain_link, sizeof(captain_link));
    }
    int include_roster = title != NULL && strstr(title, "Roster") != NULL;
    int include_samples = include_roster;

    int len = snprintf(
        out,
        out_size,
        "%s ",
        lead != NULL && lead[0] != '\0' ? lead : "The KBO issued an Asian Games update.");
    if (len < 0) {
        out[0] = '\0';
        return;
    }

    size_t used = strlen(out);
    if (title != NULL && strstr(title, "Roster") != NULL) {
        len = snprintf(
            out + used,
            out_size - used,
            "The Korean baseball delegation has settled on a %ld-man squad for the Asian Games, balancing young domestic standouts with %d over-age wild-card selection%s. League officials framed the group as a tournament roster built for short-series flexibility rather than a straight all-star list.\n\nThe first wave of attention is expected to fall on ",
            roster_count,
            wildcards,
            wildcards == 1 ? "" : "s");
    } else if (title != NULL && strstr(title, "Depart") != NULL) {
        len = snprintf(
            out + used,
            out_size - used,
            "%d player%s will now leave club duty for the Asian Games window, forcing several teams to cover short-term roster holes during a sensitive part of the league calendar. The delegation will gather before moving into tournament preparation, with the KBO saying clubs have already been notified of the final travel list.",
            departed,
            departed == 1 ? "" : "s");
    } else if (title != NULL && (strstr(title, "Return") != NULL || strstr(title, "Gold") != NULL || strstr(title, "Win") != NULL)) {
        len = snprintf(
            out + used,
            out_size - used,
            "%d player%s returned to their clubs after the Asian Games final, with %d player%s receiving military exemption status as part of Korea's championship result. Teams can now fold the group back into the pennant race and postseason picture.",
            returned,
            returned == 1 ? "" : "s",
            exempted,
            exempted == 1 ? "" : "s");
    } else {
        len = snprintf(
            out + used,
            out_size - used,
            "Korea's Asian Games group stands at %ld players for %u.\n\nThe squad includes ",
            roster_count,
            g_kbo_asian_games_roster_year);
    }
    if (len <= 0) {
        return;
    }
    used += (size_t)len;

    if (include_samples) {
        LONG sample_count = roster_count < 6 ? roster_count : 6;
        for (LONG i = 0; i < sample_count && used + 160u < out_size; i++) {
            LONG roster_index = (i == sample_count - 1 && roster_count > sample_count)
                ? roster_count - 1
                : i;
            kbo_asian_games_append_player_blurb(
                out,
                out_size,
                &used,
                &g_kbo_asian_games_roster[roster_index],
                i,
                sample_count);
        }
    }

    if (used < out_size - 1) {
        len = snprintf(
            out + used,
            out_size - used,
            "%s\n\nThe league said the final decisions reflected player availability, age rules and positional balance, with clubs notified before the roster was made public.",
            include_samples ? "." : "");
        if (len > 0) {
            used += (size_t)len;
        }
    }

    if (include_roster && captain_link[0] != '\0' && used < out_size - 1) {
        len = snprintf(
            out + used,
            out_size - used,
            "\n\n%s, named captain after being selected as one of the wild-card players, said the older players understand what is expected of them. \"This roster belongs to the young players,\" he said. \"Our job is to make the game slow down for them and make sure Korea is ready from the first pitch.\"",
            captain_link);
        if (len > 0) {
            used += (size_t)len;
        }
    }

    if (include_roster && used < out_size - 1) {
        len = snprintf(out + used, out_size - used, "\n\nComplete roster:\n");
        if (len > 0) {
            used += (size_t)len;
        }
    }

    if (include_roster) {
        for (LONG i = 0; i < roster_count && used + 140u < out_size; i++) {
            kbo_asian_games_append_roster_line(
                out,
                out_size,
                &used,
                i,
                &g_kbo_asian_games_roster[i]);
        }
    }
}

static int kbo_emit_asian_games_news(uint32_t event_yyyymmdd, const char* title, const char* lead, const char* source)
{
    if (event_yyyymmdd == 0u || title == NULL || title[0] == '\0') {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        append_logf(
            "KBO Asian Games news skipped source=%s title=%s reason=league_id_unavailable",
            source != NULL ? source : "",
            title);
        return 0;
    }

    char body[8192] = {0};
    kbo_build_asian_games_news_body(body, sizeof(body), title, lead);
    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "KBO Asian Games news source=%s title=%s date=%u league_id=%u created=%d",
        source != NULL ? source : "",
        title,
        event_yyyymmdd,
        league_id,
        created);
    return created;
}

static int kbo_emit_asian_games_replacement_news(
    uint32_t event_yyyymmdd,
    KboAsianGamesRosterEntry* old_entry,
    KboAsianGamesRosterEntry* new_entry,
    const char* source)
{
    if (event_yyyymmdd == 0u || old_entry == NULL || new_entry == NULL) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        return 0;
    }

    char old_player[96] = {0};
    char new_player[96] = {0};
    char old_team[128] = {0};
    char new_team[128] = {0};
    kbo_copy_asian_games_player_link(old_entry, old_player, sizeof(old_player));
    kbo_copy_asian_games_player_link(new_entry, new_player, sizeof(new_player));
    kbo_copy_asian_games_team_link(old_entry->original_team_id, old_team, sizeof(old_team));
    kbo_copy_asian_games_team_link(new_entry->original_team_id, new_team, sizeof(new_team));

    char body[2048] = {0};
    snprintf(
        body,
        sizeof(body),
        "The KBO announced a late change to Korea's Asian Games roster after %s%s%s was ruled unavailable through injury or roster status before the team's departure.\n\n%s%s%s has been added as the replacement, keeping the roster at %d players and preserving the same position group. League officials said the change was made before travel paperwork was finalized, allowing the national team to depart with a full tournament squad.",
        old_player,
        old_team[0] != '\0' ? " of " : "",
        old_team,
        new_player,
        new_team[0] != '\0' ? " of " : "",
        new_team,
        KBO_ASIAN_GAMES_ROSTER_SIZE);

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        "[KBO] Asian Games Roster Change",
        body);
    append_logf(
        "KBO Asian Games replacement news source=%s date=%u old_player=%u new_player=%u created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        old_entry->player_id,
        new_entry->player_id,
        created);
    return created;
}

static int kbo_handle_asian_games_selection_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_selection_fired_date == event_yyyymmdd) {
        return 0;
    }
    int selected = kbo_select_asian_games_roster(event_yyyymmdd, source);
    if (selected <= 0) {
        append_logf(
            "KBO Asian Games selection deferred source=%s date=%u selected=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            selected);
        return 0;
    }
    g_kbo_asian_games_last_selection_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        event_yyyymmdd,
        "[KBO] Asian Games Roster Announced",
        "The KBO has announced Korea's Asian Games roster.",
        source);
    append_logf(
        "KBO Asian Games selection reached source=%s date=%u selected=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        selected);
    return 1;
}

static int kbo_handle_asian_games_departure_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_departure_fired_date == event_yyyymmdd) {
        return 0;
    }
    int departed = kbo_asian_games_depart_selected_players(event_yyyymmdd, source);
    if (departed <= 0) {
        append_logf(
            "KBO Asian Games departure deferred source=%s date=%u departed=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            departed);
        return 0;
    }
    g_kbo_asian_games_last_departure_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        event_yyyymmdd,
        "[KBO] Asian Games Players Depart",
        "Selected players have departed their clubs for Asian Games duty.",
        source);
    append_logf(
        "KBO Asian Games departure reached source=%s date=%u departed=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        departed);
    return 1;
}

static int kbo_handle_asian_games_final_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_final_fired_date == event_yyyymmdd) {
        return 0;
    }
    int returned = kbo_asian_games_finalize_selected_players(event_yyyymmdd, source);
    int already_finalized = returned <= 0 ? kbo_asian_games_roster_already_finalized(source) : 0;
    if (returned <= 0 && !already_finalized) {
        append_logf(
            "KBO Asian Games final deferred source=%s date=%u returned=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            returned);
        return 0;
    }
    g_kbo_asian_games_last_final_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        event_yyyymmdd,
        "[KBO] Korea Wins Asian Games Gold",
        "Korea has won the Asian Games baseball title.",
        source);
    append_logf(
        "KBO Asian Games final reached source=%s date=%u returned=%d already_finalized=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        returned,
        already_finalized);
    return 1;
}

