/* Foreign reserve-right command execution. Included from native/src/foreign_waiver_ai.inc. */

static int kbo_execute_foreign_waiver_claim(const char* line, int line_no)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    if (line[0] == '#' || line[0] == ';') {
        return 0;
    }

    char raw[512];
    size_t raw_len = 0;
    for (; raw_len < sizeof(raw) - 1 && line[raw_len] != '\0' && line[raw_len] != '\r' && line[raw_len] != '\n'; raw_len++) {
        raw[raw_len] = line[raw_len];
    }
    raw[raw_len] = '\0';

    const char* p = raw;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    char action_name[16] = {0};
    size_t idx = 0;
    while (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t' && idx + 1 < sizeof(action_name)) {
        action_name[idx++] = *p++;
    }
    action_name[idx] = '\0';

    int retain_rights =
        strcmp(action_name, "RETAIN") == 0 || strcmp(action_name, "retain") == 0
        || strcmp(action_name, "RIGHTS") == 0 || strcmp(action_name, "rights") == 0
        || strcmp(action_name, "RESERVE") == 0 || strcmp(action_name, "reserve") == 0;
    int skip_rights =
        strcmp(action_name, "SKIP") == 0 || strcmp(action_name, "skip") == 0;

    if (strcmp(action_name, "CLAIM") != 0 && strcmp(action_name, "claim") != 0
            && strcmp(action_name, "MOVE") != 0 && strcmp(action_name, "move") != 0
            && !retain_rights && !skip_rights) {
        return 0;
    }

    if (*p == ',') {
        p++;
    } else if (*p != '\0') {
        while (*p != '\0' && *p != ',') {
            p++;
        }
        if (*p == ',') {
            p++;
        }
    }

    uint32_t team_id = 0;
    uint32_t player_id = 0;
    if (!parse_u32_from_csv_field(&p, &team_id) || !parse_u32_from_csv_field(&p, &player_id)) {
        append_logf("foreign waiver command line %d malformed: %s", line_no, raw);
        return 0;
    }

    if (team_id == 0 || player_id == 0) {
        append_logf("foreign waiver command line %d ignored (zero id): %s", line_no, raw);
        return 0;
    }

    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (destination_team == NULL) {
        append_logf("foreign waiver command line %d failed: team=%u not found", line_no, team_id);
        return 0;
    }

    uint32_t player_current_team = 0;
    uint32_t player_current_league = 0;
    uint8_t* player = kbo_find_player_by_id(player_id, &player_current_team, &player_current_league);
    if (player == NULL) {
        append_logf("foreign waiver command line %d failed: player=%u not found", line_no, player_id);
        return 0;
    }

    if (!kbo_original_club_priority_window_allows(player, team_id, action_name)) {
        return 0;
    }

    if (retain_rights) {
        uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t fallback_league = player_current_league != 0 ? player_current_league : destination_league;
        if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, team_id)) {
            append_logf("foreign waiver command line %d failed: action=%s team=%u player=%u",
                       line_no, action_name, team_id, player_id);
            return 0;
        }

        append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        kbo_append_foreign_waiver_decision_record("user", "RETAIN", team_id, player_id, 0, 0, 1);
        return 1;
    }

    if (skip_rights) {
        append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        kbo_append_foreign_waiver_decision_record("user", "SKIP", team_id, player_id, 0, 0, 1);
        return 1;
    }

    if (player_current_team == team_id) {
        append_logf("foreign waiver command line %d no-op: player=%u already on team=%u", line_no, player_id, team_id);
        return 1;
    }

    uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t fallback_league = player_current_league != 0 ? player_current_league : destination_league;
    if (fallback_league == 0) {
        return 0;
    }

    if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, team_id)) {
        append_logf("foreign waiver command line %d failed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        return 0;
    }
    append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
               line_no, action_name, team_id, player_id);
    kbo_append_foreign_waiver_decision_record("user", "RETAIN", team_id, player_id, 0, 0, 1);
    return 1;
}

