#include "../internal/amateur_player_quality_internal.h"

void kbo_lock_amateur_reputation_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_reputation_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_unlock_amateur_reputation_seeds(void)
{
    InterlockedExchange(&g_kbo_amateur_reputation_seed_lock, 0);
}

void kbo_lock_amateur_assignment_candidates(void)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_assignment_candidate_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_unlock_amateur_assignment_candidates(void)
{
    InterlockedExchange(&g_kbo_amateur_assignment_candidate_lock, 0);
}

void kbo_invalidate_amateur_assignment_candidate_cache(void)
{
    kbo_lock_amateur_assignment_candidates();
    g_kbo_amateur_assignment_high_school_count = -1;
    g_kbo_amateur_assignment_college_count = -1;
    g_kbo_amateur_resolved_team_reputation_count = 0;
    InterlockedExchange(&g_kbo_amateur_assignment_rejected_target_count, 0);
    memset(g_kbo_amateur_assignment_high_school_candidates, 0, sizeof(g_kbo_amateur_assignment_high_school_candidates));
    memset(g_kbo_amateur_assignment_college_candidates, 0, sizeof(g_kbo_amateur_assignment_college_candidates));
    memset(g_kbo_amateur_resolved_team_reputations, 0, sizeof(g_kbo_amateur_resolved_team_reputations));
    memset(g_kbo_amateur_assignment_rejected_targets, 0, sizeof(g_kbo_amateur_assignment_rejected_targets));
    kbo_unlock_amateur_assignment_candidates();
}

int kbo_get_reputation_seed_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\%s", local_app_data, file_name);
    return out[0] != '\0';
}

int kbo_amateur_reputation_add_seed(
    uint32_t league_id,
    uint32_t team_id,
    const char* team_abbr,
    const char* team_name,
    const char* nick_name,
    uint32_t reputation)
{
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }
    if (reputation < 1u) {
        reputation = 1u;
    } else if (reputation > 100u) {
        reputation = 100u;
    }

    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        if (g_kbo_amateur_reputation_seeds[i].league_id == league_id
                && g_kbo_amateur_reputation_seeds[i].team_id == team_id) {
            if (team_abbr != NULL && team_abbr[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].team_abbr, sizeof(g_kbo_amateur_reputation_seeds[i].team_abbr), "%s", team_abbr);
            }
            if (team_name != NULL && team_name[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].team_name, sizeof(g_kbo_amateur_reputation_seeds[i].team_name), "%s", team_name);
            }
            if (nick_name != NULL && nick_name[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].nick_name, sizeof(g_kbo_amateur_reputation_seeds[i].nick_name), "%s", nick_name);
            }
            g_kbo_amateur_reputation_seeds[i].reputation = (uint8_t)reputation;
            return 1;
        }
    }
    if (g_kbo_amateur_reputation_seed_count >= KBO_AMATEUR_REPUTATION_SEED_MAX) {
        return 0;
    }
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].league_id = league_id;
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_id = team_id;
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_abbr, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_abbr), "%s", team_abbr != NULL ? team_abbr : "");
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_name, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_name), "%s", team_name != NULL ? team_name : "");
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].nick_name, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].nick_name), "%s", nick_name != NULL ? nick_name : "");
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].reputation = (uint8_t)reputation;
    g_kbo_amateur_reputation_seed_count++;
    return 1;
}

int kbo_load_reputation_seed_path_into_cache(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    char* buffer = NULL;
    DWORD size = 0;
    if (!kbo_read_amateur_reputation_seed_file(path, &buffer, &size)) {
        append_logf("team reputation seed load skipped path=%s reason=read_failed", path);
        return 0;
    }

    int before = g_kbo_amateur_reputation_seed_count;
    char* cursor = buffer;
    while (*cursor != '\0') {
        char* line = cursor;
        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }
        if (*cursor == '\n') {
            *cursor++ = '\0';
        }
        if (line[0] == '#' || line[0] == '\0' || strstr(line, "league_id,") == line) {
            continue;
        }

        const char* cell_cursor = line;
        char cell[128] = {0};
        uint32_t league_id = 0u;
        uint32_t team_id = 0u;
        char team_abbr[32] = {0};
        char team_name[96] = {0};
        char nick_name[64] = {0};
        uint32_t reputation = 0u;
        for (int col = 0; col <= 9; col++) {
            kbo_amateur_reputation_read_cell(&cell_cursor, cell, sizeof(cell));
            if (col == 0) {
                league_id = kbo_amateur_reputation_parse_u32(cell);
            } else if (col == 5) {
                team_id = kbo_amateur_reputation_parse_u32(cell);
            } else if (col == 6) {
                snprintf(team_abbr, sizeof(team_abbr), "%s", cell);
            } else if (col == 7) {
                snprintf(team_name, sizeof(team_name), "%s", cell);
            } else if (col == 8) {
                snprintf(nick_name, sizeof(nick_name), "%s", cell);
            } else if (col == 9) {
                reputation = kbo_amateur_reputation_parse_u32(cell);
            }
        }
        kbo_amateur_reputation_add_seed(league_id, team_id, team_abbr, team_name, nick_name, reputation);
    }

    int added = g_kbo_amateur_reputation_seed_count - before;
    HeapFree(GetProcessHeap(), 0, buffer);
    append_logf("team reputation seed loaded added=%d total=%d path=%s", added, g_kbo_amateur_reputation_seed_count, path);
    return 1;
}

int kbo_get_amateur_reputation_history_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("amateur_reputation_history.csv", out, out_size);
}

int kbo_amateur_reputation_history_has_year(uint32_t league_id, uint32_t year)
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

int kbo_apply_amateur_reputation_history_to_cache(void)
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

void kbo_ensure_amateur_reputation_seeds_loaded(void)
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

int kbo_find_amateur_team_reputation_for_league(uint32_t league_id, uint32_t team_id, uint8_t* out_reputation)
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

