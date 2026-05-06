/* All-Star team-split CSV path, storage, and loading. */

static int get_kbo_allstar_teams_csv_path(char* path, size_t path_size)
{
    if (path == NULL || path_size == 0) {
        return 0;
    }
    path[0] = '\0';

    HMODULE exe = GetModuleHandleA(NULL);
    char host_stats_dir[MAX_PATH] = {0};
    if (exe != NULL) {
        char host[MAX_PATH] = {0};
        DWORD got = GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
        if (got > 0 && got < sizeof(host)) {
            char* slash = strrchr(host, '\\');
            if (slash != NULL) {
                *slash = '\0';
                snprintf(host_stats_dir, sizeof(host_stats_dir), "%s\\data\\stats\\KBO", host);
                snprintf(path, path_size, "%s\\allstar_teams.csv", host_stats_dir);
                if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
                    return 1;
                }
            }
        }
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got > 0 && got < sizeof(local_app_data)) {
        snprintf(path, path_size, "%s\\OOTP-KBO\\allstar_teams.csv", local_app_data);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    if (host_stats_dir[0] != '\0') {
        snprintf(path, path_size, "%s\\Teams.csv", host_stats_dir);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    if (got > 0 && got < sizeof(local_app_data)) {
        snprintf(path, path_size, "%s\\OOTP-KBO\\Teams.csv", local_app_data);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    return path[0] != '\0';
}

static void add_allstar_team_row(uint16_t year, const char* team_id, const char* team_name, const char* current_city, uint8_t side)
{
    if (year < 1800 || year > 2200 || side == 0 || side > 2
            || team_id == NULL || team_id[0] == '\0'
            || g_allstar_team_row_count >= OOTP27_KBO_MAX_ALLSTAR_TEAM_ROWS) {
        return;
    }

    for (int i = 0; i < g_allstar_team_row_count; i++) {
        KboAllstarTeamRow* existing = &g_allstar_team_rows[i];
        if (existing->year == year
                && ascii_equals_ignore_case(existing->team_id, team_id)
                && existing->side == side) {
            return;
        }
    }

    KboAllstarTeamRow* row = &g_allstar_team_rows[g_allstar_team_row_count++];
    row->year = year;
    row->side = side;
    snprintf(row->team_id, sizeof(row->team_id), "%s", team_id);
    snprintf(row->team_name, sizeof(row->team_name), "%s", team_name != NULL ? team_name : "");
    snprintf(row->current_city, sizeof(row->current_city), "%s", current_city != NULL ? current_city : "");
}

static void load_allstar_team_rules_once(void)
{
    if (InterlockedCompareExchange(&g_allstar_team_rules_loaded, 1, 0) != 0) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_allstar_teams_csv_path(path, sizeof(path))) {
        append_log_line("all-star team split load failed: could not resolve allstar_teams.csv path");
        return;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        append_logf("all-star team split load skipped: fopen failed path=%s", path);
        return;
    }

    char line[8192] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        append_logf("all-star team split load failed: empty file path=%s", path);
        return;
    }

    int year_col = -1;
    int team_id_col = -1;
    int name_col = -1;
    int allstar_col = -1;
    kbo_csv_find_allstar_team_columns(line, &year_col, &team_id_col, &name_col, &allstar_col);
    if (year_col < 0 || team_id_col < 0 || allstar_col < 0) {
        fclose(file);
        append_logf(
            "all-star team split load skipped: all-star CSV missing columns year_col=%d team_id_col=%d name_col=%d allstar_col=%d path=%s",
            year_col,
            team_id_col,
            name_col,
            allstar_col,
            path);
        return;
    }

    int rows = 0;
    int loaded = 0;
    int invalid_side = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        uint16_t year = 0;
        char team_id[16] = {0};
        char team_name[96] = {0};
        char current_city[64] = {0};
        uint8_t side = 0;
        kbo_csv_extract_allstar_team_fields(
            line,
            year_col,
            team_id_col,
            name_col,
            allstar_col,
            &year,
            team_id,
            sizeof(team_id),
            team_name,
            sizeof(team_name),
            current_city,
            sizeof(current_city),
            &side);
        rows++;
        if (year == 0 || team_id[0] == '\0') {
            continue;
        }
        if (side == 0) {
            invalid_side++;
            continue;
        }

        int before = g_allstar_team_row_count;
        add_allstar_team_row(year, team_id, team_name, current_city, side);
        if (g_allstar_team_row_count != before) {
            loaded++;
        }
    }

    fclose(file);
    append_logf(
        "all-star team split load: rows=%d loaded=%d invalid_side=%d total=%d year_col=%d team_id_col=%d name_col=%d allstar_col=%d path=%s",
        rows,
        loaded,
        invalid_side,
        g_allstar_team_row_count,
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        path);
}
