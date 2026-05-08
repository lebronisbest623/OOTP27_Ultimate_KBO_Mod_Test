static int kbo_get_amateur_reputation_history_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("amateur_reputation_history.csv", out, out_size);
}

static int kbo_amateur_reputation_history_has_year(uint32_t league_id, uint32_t year)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_amateur_reputation_history_path(path, sizeof(path))) {
        return 0;
    }

    char* buffer = NULL;
    DWORD size = 0;
    if (!kbo_read_amateur_reputation_seed_file(path, &buffer, &size)) {
        return 0;
    }

    int found = 0;
    char* cursor = buffer;
    while (*cursor != '\0') {
        char* line = cursor;
        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }
        if (*cursor == '\n') {
            *cursor++ = '\0';
        }
        if (line[0] == '#' || line[0] == '\0' || strstr(line, "year,") == line) {
            continue;
        }

        const char* cell_cursor = line;
        char cell[128] = {0};
        uint32_t row_year = 0u;
        uint32_t row_league_id = 0u;
        for (int col = 0; col <= 1; col++) {
            kbo_amateur_reputation_read_cell(&cell_cursor, cell, sizeof(cell));
            if (col == 0) {
                row_year = kbo_amateur_reputation_parse_u32(cell);
            } else if (col == 1) {
                row_league_id = kbo_amateur_reputation_parse_u32(cell);
            }
        }
        if (row_year == year && row_league_id == league_id) {
            found = 1;
            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static int kbo_apply_amateur_reputation_history_to_cache(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_amateur_reputation_history_path(path, sizeof(path))) {
        return 0;
    }

    char* buffer = NULL;
    DWORD size = 0;
    if (!kbo_read_amateur_reputation_seed_file(path, &buffer, &size)) {
        return 0;
    }

    int applied = 0;
    char* cursor = buffer;
    while (*cursor != '\0') {
        char* line = cursor;
        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }
        if (*cursor == '\n') {
            *cursor++ = '\0';
        }
        if (line[0] == '#' || line[0] == '\0' || strstr(line, "year,") == line) {
            continue;
        }

        const char* cell_cursor = line;
        char cell[128] = {0};
        uint32_t league_id = 0u;
        uint32_t team_id = 0u;
        uint32_t new_reputation = 0u;
        for (int col = 0; col <= 6; col++) {
            kbo_amateur_reputation_read_cell(&cell_cursor, cell, sizeof(cell));
            if (col == 1) {
                league_id = kbo_amateur_reputation_parse_u32(cell);
            } else if (col == 2) {
                team_id = kbo_amateur_reputation_parse_u32(cell);
            } else if (col == 6) {
                new_reputation = kbo_amateur_reputation_parse_u32(cell);
            }
        }
        if (league_id != 0u && team_id != 0u && new_reputation != 0u
                && kbo_amateur_reputation_add_seed(league_id, team_id, "", "", "", new_reputation)) {
            applied++;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    append_logf("amateur reputation history replayed rows=%d path=%s", applied, path);
    return applied;
}

static void kbo_ensure_amateur_reputation_seeds_loaded(void)
{
    char high_school_path[MAX_PATH] = {0};
    char college_path[MAX_PATH] = {0};
    char history_path[MAX_PATH] = {0};
    kbo_get_reputation_seed_path("high_school_reputation_seed.csv", high_school_path, sizeof(high_school_path));
    kbo_get_reputation_seed_path("college_reputation_seed.csv", college_path, sizeof(college_path));
    kbo_get_amateur_reputation_history_path(history_path, sizeof(history_path));

    char loaded_key[MAX_PATH * 3] = {0};
    snprintf(loaded_key, sizeof(loaded_key), "%s|%s|%s", high_school_path, college_path, history_path);
    if (InterlockedCompareExchange(&g_kbo_amateur_reputation_seed_loaded, 1, 1) == 1
            && strcmp(loaded_key, g_kbo_amateur_reputation_seed_loaded_path) == 0) {
        return;
    }

    kbo_lock_amateur_reputation_seeds();
    memset(g_kbo_amateur_reputation_seeds, 0, sizeof(g_kbo_amateur_reputation_seeds));
    g_kbo_amateur_reputation_seed_count = 0;
    if (high_school_path[0] != '\0') {
        kbo_load_reputation_seed_path_into_cache(high_school_path);
    }
    if (college_path[0] != '\0') {
        kbo_load_reputation_seed_path_into_cache(college_path);
    }
    kbo_apply_amateur_reputation_history_to_cache();
    strncpy(g_kbo_amateur_reputation_seed_loaded_path, loaded_key, sizeof(g_kbo_amateur_reputation_seed_loaded_path) - 1u);
    g_kbo_amateur_reputation_seed_loaded_path[sizeof(g_kbo_amateur_reputation_seed_loaded_path) - 1u] = '\0';
    InterlockedExchange(&g_kbo_amateur_reputation_seed_loaded, 1);
    int count = g_kbo_amateur_reputation_seed_count;
    kbo_unlock_amateur_reputation_seeds();
    append_logf("team reputation seeds ready count=%d", count);
}

static int kbo_find_amateur_team_reputation_for_league(uint32_t league_id, uint32_t team_id, uint8_t* out_reputation)
{
    if (out_reputation != NULL) {
        *out_reputation = 70u;
    }
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }
    kbo_ensure_amateur_reputation_seeds_loaded();
    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        if (g_kbo_amateur_reputation_seeds[i].league_id == league_id
                && g_kbo_amateur_reputation_seeds[i].team_id == team_id) {
            if (out_reputation != NULL) {
                *out_reputation = g_kbo_amateur_reputation_seeds[i].reputation;
            }
            kbo_unlock_amateur_reputation_seeds();
            return 1;
        }
    }
    kbo_unlock_amateur_reputation_seeds();
    return 0;
}
