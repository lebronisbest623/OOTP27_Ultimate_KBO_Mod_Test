typedef struct KboHubReputationHistoryRow {
    uint32_t year;
    uint32_t league_id;
    uint32_t team_id;
    int32_t old_reputation;
    int32_t delta;
    int32_t new_reputation;
    int32_t wins;
    int32_t losses;
    int32_t ties;
    int32_t score;
    int32_t rank;
} KboHubReputationHistoryRow;

typedef struct KboHubReputationTeamRow {
    uint32_t team_id;
    char name[128];
    int32_t current_reputation;
    int32_t latest_rank;
    int32_t latest_wins;
    int32_t latest_losses;
    int32_t latest_ties;
    int32_t year_reputation[5];
    int32_t year_delta[5];
    int has_year[5];
} KboHubReputationTeamRow;

static int kbo_hub_compare_reputation_history_rows(const void* a, const void* b)
{
    const KboHubReputationHistoryRow* left = (const KboHubReputationHistoryRow*)a;
    const KboHubReputationHistoryRow* right = (const KboHubReputationHistoryRow*)b;
    if (left->year != right->year) {
        return left->year < right->year ? 1 : -1;
    }
    if (left->rank != right->rank) {
        return left->rank > right->rank ? 1 : -1;
    }
    if (left->new_reputation != right->new_reputation) {
        return left->new_reputation < right->new_reputation ? 1 : -1;
    }
    return 0;
}

static int kbo_hub_compare_reputation_team_rows(const void* a, const void* b)
{
    const KboHubReputationTeamRow* left = (const KboHubReputationTeamRow*)a;
    const KboHubReputationTeamRow* right = (const KboHubReputationTeamRow*)b;
    if (left->current_reputation != right->current_reputation) {
        return left->current_reputation < right->current_reputation ? 1 : -1;
    }
    return _stricmp(left->name, right->name);
}

static int kbo_hub_find_reputation_team_row(KboHubReputationTeamRow* teams, int team_count, uint32_t team_id)
{
    for (int i = 0; i < team_count; i++) {
        if (teams[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_hub_load_reputation_team_rows(uint32_t league_id, KboHubReputationTeamRow* teams, int max_teams)
{
    if (league_id == 0u || teams == NULL || max_teams <= 0) {
        return 0;
    }

    kbo_ensure_amateur_reputation_seeds_loaded();
    int team_count = 0;
    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < g_kbo_amateur_reputation_seed_count && team_count < max_teams; i++) {
        KboAmateurReputationSeed* seed = &g_kbo_amateur_reputation_seeds[i];
        if (seed->league_id != league_id || seed->team_id == 0u) {
            continue;
        }
        KboHubReputationTeamRow* team = &teams[team_count++];
        memset(team, 0, sizeof(*team));
        team->team_id = seed->team_id;
        team->current_reputation = seed->reputation;
        team->latest_rank = 9999;
        if (seed->team_name[0] != '\0' && seed->nick_name[0] != '\0') {
            snprintf(team->name, sizeof(team->name), "%s %s", seed->team_name, seed->nick_name);
        } else if (seed->team_name[0] != '\0') {
            snprintf(team->name, sizeof(team->name), "%s", seed->team_name);
        } else if (seed->nick_name[0] != '\0') {
            snprintf(team->name, sizeof(team->name), "%s", seed->nick_name);
        } else if (seed->team_abbr[0] != '\0') {
            snprintf(team->name, sizeof(team->name), "%s", seed->team_abbr);
        } else {
            snprintf(team->name, sizeof(team->name), "Team %u", seed->team_id);
        }
    }
    kbo_unlock_amateur_reputation_seeds();
    qsort(teams, (size_t)team_count, sizeof(teams[0]), kbo_hub_compare_reputation_team_rows);
    return team_count;
}

static int kbo_hub_load_reputation_history_rows(
    uint32_t league_id,
    KboHubReputationHistoryRow* rows,
    int max_rows,
    uint32_t* selected_years,
    int* out_selected_year_count)
{
    if (out_selected_year_count != NULL) {
        *out_selected_year_count = 0;
    }
    if (league_id == 0u || rows == NULL || max_rows <= 0 || selected_years == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_amateur_reputation_history_path(path, sizeof(path))) {
        return 0;
    }

    char* buffer = NULL;
    DWORD size = 0;
    if (!kbo_read_amateur_reputation_seed_file(path, &buffer, &size) || buffer == NULL) {
        return 0;
    }

    int row_count = 0;
    uint32_t years[64] = {0};
    int year_count = 0;
    char* cursor = buffer;
    while (cursor != NULL && *cursor != '\0') {
        char* line = cursor;
        char* newline = strpbrk(cursor, "\r\n");
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
            while (*cursor == '\r' || *cursor == '\n') {
                cursor++;
            }
        } else {
            cursor = NULL;
        }

        if (line[0] == '\0' || line[0] == '#' || strncmp(line, "year,", 5) == 0) {
            continue;
        }

        const char* cell_cursor = line;
        char cell[128] = {0};
        KboHubReputationHistoryRow row;
        memset(&row, 0, sizeof(row));
        for (int col = 0; col < 12; col++) {
            kbo_amateur_reputation_read_cell(&cell_cursor, cell, sizeof(cell));
            switch (col) {
            case 0: row.year = kbo_amateur_reputation_parse_u32(cell); break;
            case 1: row.league_id = kbo_amateur_reputation_parse_u32(cell); break;
            case 2: row.team_id = kbo_amateur_reputation_parse_u32(cell); break;
            case 3: row.old_reputation = (int32_t)strtol(cell, NULL, 10); break;
            case 4: row.delta = (int32_t)strtol(cell, NULL, 10); break;
            case 5: row.new_reputation = (int32_t)strtol(cell, NULL, 10); break;
            case 6: row.wins = (int32_t)strtol(cell, NULL, 10); break;
            case 7: row.losses = (int32_t)strtol(cell, NULL, 10); break;
            case 8: row.ties = (int32_t)strtol(cell, NULL, 10); break;
            case 9: row.score = (int32_t)strtol(cell, NULL, 10); break;
            case 10: row.rank = (int32_t)strtol(cell, NULL, 10); break;
            default: break;
            }
        }

        if (row.league_id != league_id || row.year == 0u || row.team_id == 0u) {
            continue;
        }
        if (row_count < max_rows) {
            rows[row_count++] = row;
        }
        int exists = 0;
        for (int i = 0; i < year_count; i++) {
            if (years[i] == row.year) {
                exists = 1;
                break;
            }
        }
        if (!exists && year_count < (int)(sizeof(years) / sizeof(years[0]))) {
            years[year_count++] = row.year;
        }
    }
    HeapFree(GetProcessHeap(), 0, buffer);

    for (int i = 0; i < year_count; i++) {
        for (int j = i + 1; j < year_count; j++) {
            if (years[i] < years[j]) {
                uint32_t tmp = years[i];
                years[i] = years[j];
                years[j] = tmp;
            }
        }
    }
    int selected_count = year_count < 5 ? year_count : 5;
    for (int i = 0; i < selected_count; i++) {
        selected_years[i] = years[i];
    }
    if (out_selected_year_count != NULL) {
        *out_selected_year_count = selected_count;
    }
    qsort(rows, (size_t)row_count, sizeof(rows[0]), kbo_hub_compare_reputation_history_rows);
    return row_count;
}

static void kbo_webview_append_reputation_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    KboHubReputationTeamRow teams[512];
    KboHubReputationHistoryRow rows[4096];
    uint32_t selected_years[5] = {0};
    int selected_year_count = 0;
    int team_count = kbo_hub_load_reputation_team_rows(
        g_kbo_hub_selected_league_id,
        teams,
        (int)(sizeof(teams) / sizeof(teams[0])));
    int row_count = kbo_hub_load_reputation_history_rows(
        g_kbo_hub_selected_league_id,
        rows,
        (int)(sizeof(rows) / sizeof(rows[0])),
        selected_years,
        &selected_year_count);

    for (int i = 0; i < row_count; i++) {
        KboHubReputationHistoryRow* row = &rows[i];
        int team_index = kbo_hub_find_reputation_team_row(teams, team_count, row->team_id);
        if (team_index < 0) {
            continue;
        }
        for (int y = 0; y < selected_year_count; y++) {
            if (row->year == selected_years[y]) {
                teams[team_index].year_reputation[y] = row->new_reputation;
                teams[team_index].year_delta[y] = row->delta;
                teams[team_index].has_year[y] = 1;
                if (y == 0) {
                    teams[team_index].latest_rank = row->rank;
                    teams[team_index].latest_wins = row->wins;
                    teams[team_index].latest_losses = row->losses;
                    teams[team_index].latest_ties = row->ties;
                }
                break;
            }
        }
    }

    kbo_window_text_appendf(
        buffer,
        "<div class='rights rosterRights'><section class='tablewrap rosterTableWrap'><table class='ootpRosterTable reputationTable'><thead><tr>"
        "<th data-sort-type='text'>Team</th><th style='width:88px' data-sort-type='number'>Current</th>"
        "<th style='width:64px' data-sort-type='number'>Rank</th><th style='width:92px' data-sort-type='text'>Record</th>");
    for (int i = 0; i < selected_year_count; i++) {
        kbo_window_text_appendf(buffer, "<th style='width:96px' data-sort-type='number'>%u</th>", selected_years[i]);
    }
    kbo_window_text_appendf(buffer, "</tr></thead><tbody>");

    for (int i = 0; i < team_count; i++) {
        KboHubReputationTeamRow* team = &teams[i];
        kbo_window_text_appendf(
            buffer,
            "<tr><td class='pname'>");
        kbo_html_append_escaped(buffer, team->name);
        kbo_window_text_appendf(
            buffer,
            "</td><td data-sort-value='%d'>%d</td><td data-sort-value='%d'>",
            team->current_reputation,
            team->current_reputation,
            team->latest_rank >= 9999 ? 9999 : team->latest_rank);
        if (team->latest_rank >= 9999) {
            kbo_window_text_appendf(buffer, "-");
        } else {
            kbo_window_text_appendf(buffer, "%d", team->latest_rank);
        }
        kbo_window_text_appendf(
            buffer,
            "</td><td data-sort-value='%03d-%03d-%03d'>%d-%d-%d</td>",
            team->latest_wins,
            team->latest_losses,
            team->latest_ties,
            team->latest_wins,
            team->latest_losses,
            team->latest_ties);
        for (int y = 0; y < selected_year_count; y++) {
            if (!team->has_year[y]) {
                kbo_window_text_appendf(buffer, "<td data-sort-value='-1'>-</td>");
                continue;
            }
            const char* delta_class = team->year_delta[y] > 0 ? "pos" : (team->year_delta[y] < 0 ? "neg" : "even");
            kbo_window_text_appendf(
                buffer,
                "<td class='%s' data-sort-value='%d'>%d (%+d)</td>",
                delta_class,
                team->year_reputation[y],
                team->year_reputation[y],
                team->year_delta[y]);
        }
        kbo_window_text_appendf(buffer, "</tr>");
    }

    if (team_count == 0) {
        kbo_window_text_appendf(
            buffer,
            "<tr><td colspan='4' class='roEmptyMessage'>No reputation seed is available for this league.</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
