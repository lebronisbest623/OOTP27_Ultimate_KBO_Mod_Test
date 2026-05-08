/* Amateur player assignment rerouting from team reputation seed. */

#define KBO_COLLEGE_LEAGUE_ID 201u
#define KBO_HIGH_SCHOOL_LEAGUE_ID 203u
#define KBO_AMATEUR_REPUTATION_SEED_MAX 384
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX 16384
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX 32768
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK (KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX - 1)
#define KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX 128
#define KBO_AMATEUR_REPUTATION_UPDATE_MIN_GAMES 10u
#define KBO_COLLEGE_ASSIGNMENT_SOURCE_MIN_PLAYERS 24
#define KBO_AMATEUR_TEAM_REGULAR_WINS_OFFSET   0x400cu
#define KBO_AMATEUR_TEAM_REGULAR_LOSSES_OFFSET 0x4018u
#define KBO_AMATEUR_TEAM_REGULAR_TIES_OFFSET   0x401cu

typedef struct KboAmateurReputationSeed {
    uint32_t league_id;
    uint32_t team_id;
    char team_abbr[32];
    char team_name[96];
    char nick_name[64];
    uint8_t reputation;
} KboAmateurReputationSeed;

typedef struct KboAmateurReputationUpdateRow {
    uint32_t league_id;
    uint32_t team_id;
    uint8_t old_reputation;
    uint8_t new_reputation;
    uint16_t wins;
    uint16_t losses;
    uint16_t ties;
    int32_t score;
} KboAmateurReputationUpdateRow;

typedef struct KboAmateurAssignmentProcessed {
    uint32_t player_id;
    uint32_t team_id;
} KboAmateurAssignmentProcessed;

#define KBO_AMATEUR_ASSIGNMENT_TEAM_MAX 512

typedef struct KboAmateurAssignmentCandidate {
    uint8_t* team;
    uint32_t team_id;
    uint8_t reputation;
    int32_t player_count;
} KboAmateurAssignmentCandidate;

typedef struct KboAmateurResolvedTeamReputation {
    uint8_t* team;
    uint32_t league_id;
    uint8_t reputation;
} KboAmateurResolvedTeamReputation;

static KboAmateurReputationSeed g_kbo_amateur_reputation_seeds[KBO_AMATEUR_REPUTATION_SEED_MAX];
static int g_kbo_amateur_reputation_seed_count = 0;
static LONG g_kbo_amateur_reputation_seed_lock = 0;
static LONG g_kbo_amateur_reputation_seed_loaded = 0;
static char g_kbo_amateur_reputation_seed_loaded_path[MAX_PATH * 3] = {0};
static KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed[KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX];
static KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed_hash[KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX];
static LONG g_kbo_amateur_assignment_processed_count = 0;
static LONG g_kbo_amateur_assignment_processed_hash_count = 0;
static KboAmateurAssignmentCandidate g_kbo_amateur_assignment_high_school_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
static KboAmateurAssignmentCandidate g_kbo_amateur_assignment_college_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
static KboAmateurAssignmentProcessed g_kbo_amateur_assignment_rejected_targets[KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX];
static int g_kbo_amateur_assignment_high_school_count = -1;
static int g_kbo_amateur_assignment_college_count = -1;
static LONG g_kbo_amateur_assignment_rejected_target_count = 0;
static LONG g_kbo_amateur_assignment_candidate_lock = 0;
static KboAmateurResolvedTeamReputation g_kbo_amateur_resolved_team_reputations[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
static int g_kbo_amateur_resolved_team_reputation_count = 0;
static uint32_t g_kbo_amateur_reputation_last_update_high_school_year = 0u;
static uint32_t g_kbo_amateur_reputation_last_update_college_year = 0u;

static void kbo_lock_amateur_reputation_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_reputation_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_amateur_reputation_seeds(void)
{
    InterlockedExchange(&g_kbo_amateur_reputation_seed_lock, 0);
}

static void kbo_lock_amateur_assignment_candidates(void)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_assignment_candidate_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_amateur_assignment_candidates(void)
{
    InterlockedExchange(&g_kbo_amateur_assignment_candidate_lock, 0);
}

static int* kbo_amateur_assignment_count_ptr_for_league(uint32_t league_id)
{
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return &g_kbo_amateur_assignment_high_school_count;
    }
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return &g_kbo_amateur_assignment_college_count;
    }
    return NULL;
}

static KboAmateurAssignmentCandidate* kbo_amateur_assignment_cache_for_league(uint32_t league_id)
{
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return g_kbo_amateur_assignment_high_school_candidates;
    }
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return g_kbo_amateur_assignment_college_candidates;
    }
    return NULL;
}

static void kbo_invalidate_amateur_assignment_candidate_cache(void)
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

static void kbo_amateur_reputation_trim_cell(char* value)
{
    if (value == NULL) {
        return;
    }
    char* start = value;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '"') {
        start++;
    }
    if (start != value) {
        memmove(value, start, strlen(start) + 1u);
    }
    size_t len = strlen(value);
    while (len > 0u) {
        char c = value[len - 1u];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '"') {
            break;
        }
        value[--len] = '\0';
    }
}

static void kbo_amateur_reputation_read_cell(const char** cursor, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (cursor == NULL || *cursor == NULL) {
        return;
    }

    const char* p = *cursor;
    size_t len = 0u;
    int quoted = 0;
    if (*p == '"') {
        quoted = 1;
        p++;
    }
    while (*p != '\0') {
        if (quoted) {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (len + 1u < out_size) {
                        out[len++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                if (*p == ',') {
                    p++;
                }
                break;
            }
        } else if (*p == ',') {
            p++;
            break;
        } else if (*p == '\r' || *p == '\n') {
            break;
        }
        if (len + 1u < out_size) {
            out[len++] = *p;
        }
        p++;
    }
    out[len] = '\0';
    kbo_amateur_reputation_trim_cell(out);
    *cursor = p;
}

static uint32_t kbo_amateur_reputation_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    unsigned long value = strtoul(text, NULL, 10);
    if (value > UINT32_MAX) {
        return 0u;
    }
    return (uint32_t)value;
}

static int kbo_get_reputation_seed_path(const char* file_name, char* out, size_t out_size)
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

static int kbo_read_amateur_reputation_seed_file(const char* path, char** out_buffer, DWORD* out_size)
{
    if (out_buffer == NULL || out_size == NULL) {
        return 0;
    }
    *out_buffer = NULL;
    *out_size = 0;
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    LARGE_INTEGER file_size = {0};
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }

    DWORD size = (DWORD)file_size.QuadPart;
    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int ok = ReadFile(file, buffer, size, &read, NULL) && read == size;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    buffer[size] = '\0';
    *out_buffer = buffer;
    *out_size = size;
    return 1;
}

static int kbo_amateur_reputation_add_seed(
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

static int kbo_load_reputation_seed_path_into_cache(const char* path)
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

static int kbo_find_amateur_team_reputation_by_memory_team(uint32_t league_id, uint8_t* team, uint8_t* out_reputation)
{
    if (out_reputation != NULL) {
        *out_reputation = 70u;
    }
    if (league_id == 0u || team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_amateur_resolved_team_reputation_count; i++) {
        if (g_kbo_amateur_resolved_team_reputations[i].team == team
                && g_kbo_amateur_resolved_team_reputations[i].league_id == league_id) {
            if (out_reputation != NULL) {
                *out_reputation = g_kbo_amateur_resolved_team_reputations[i].reputation;
            }
            return 1;
        }
    }
    uint32_t numeric_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (kbo_find_amateur_team_reputation_for_league(league_id, numeric_team_id, out_reputation)) {
        if (g_kbo_amateur_resolved_team_reputation_count < KBO_AMATEUR_ASSIGNMENT_TEAM_MAX) {
            KboAmateurResolvedTeamReputation* resolved = &g_kbo_amateur_resolved_team_reputations[g_kbo_amateur_resolved_team_reputation_count++];
            resolved->team = team;
            resolved->league_id = league_id;
            resolved->reputation = out_reputation != NULL ? *out_reputation : 70u;
        }
        return 1;
    }

    kbo_ensure_amateur_reputation_seeds_loaded();
    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        KboAmateurReputationSeed* seed = &g_kbo_amateur_reputation_seeds[i];
        if (seed->league_id != league_id) {
            continue;
        }
        if ((seed->team_abbr[0] != '\0' && team_has_ootp_string_text(team, seed->team_abbr))
                || (seed->team_name[0] != '\0' && team_has_ootp_string_text(team, seed->team_name))) {
            if (out_reputation != NULL) {
                *out_reputation = seed->reputation;
            }
            if (g_kbo_amateur_resolved_team_reputation_count < KBO_AMATEUR_ASSIGNMENT_TEAM_MAX) {
                KboAmateurResolvedTeamReputation* resolved = &g_kbo_amateur_resolved_team_reputations[g_kbo_amateur_resolved_team_reputation_count++];
                resolved->team = team;
                resolved->league_id = league_id;
                resolved->reputation = seed->reputation;
            }
            kbo_unlock_amateur_reputation_seeds();
            return 1;
        }
    }
    kbo_unlock_amateur_reputation_seeds();
    return 0;
}

static uint32_t kbo_resolve_amateur_assignment_league_id_for_team_ptr(uint8_t* team)
{
    if (team == NULL) {
        return 0u;
    }
    uint8_t reputation = 0u;
    if (kbo_find_amateur_team_reputation_by_memory_team(KBO_HIGH_SCHOOL_LEAGUE_ID, team, &reputation)) {
        return KBO_HIGH_SCHOOL_LEAGUE_ID;
    }
    if (kbo_find_amateur_team_reputation_by_memory_team(KBO_COLLEGE_LEAGUE_ID, team, &reputation)) {
        return KBO_COLLEGE_LEAGUE_ID;
    }
    return 0u;
}

static int kbo_compare_amateur_reputation_update_rows(const void* a, const void* b)
{
    const KboAmateurReputationUpdateRow* left = (const KboAmateurReputationUpdateRow*)a;
    const KboAmateurReputationUpdateRow* right = (const KboAmateurReputationUpdateRow*)b;
    if (left->score != right->score) {
        return right->score - left->score;
    }
    if (left->wins != right->wins) {
        return (int)right->wins - (int)left->wins;
    }
    return (int)left->team_id - (int)right->team_id;
}

static uint8_t kbo_amateur_reputation_clamp_for_league(uint32_t league_id, int32_t value)
{
    int32_t min_value = league_id == KBO_COLLEGE_LEAGUE_ID ? 3 : 15;
    int32_t max_value = league_id == KBO_COLLEGE_LEAGUE_ID ? 78 : 98;
    if (value < min_value) {
        value = min_value;
    } else if (value > max_value) {
        value = max_value;
    }
    return (uint8_t)value;
}

static int kbo_append_amateur_reputation_history(
    uint32_t league_id,
    const KboAmateurReputationUpdateRow* rows,
    int row_count,
    const char* source,
    uint32_t year)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_amateur_reputation_history_path(path, sizeof(path))) {
        append_logf("amateur reputation history skipped source=%s league=%u year=%u reason=no_save_scoped_path",
            source != NULL ? source : "", league_id, year);
        return 0;
    }

    int write_header = 1;
    DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        HANDLE existing = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (existing != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER size = {0};
            if (GetFileSizeEx(existing, &size) && size.QuadPart > 0) {
                write_header = 0;
            }
            CloseHandle(existing);
        }
    }

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("amateur reputation history write failed source=%s league=%u year=%u path=%s error=%lu",
            source != NULL ? source : "", league_id, year, path, GetLastError());
        return 0;
    }

    char line[512] = {0};
    DWORD written = 0;
    int ok = 1;
    if (write_header) {
        int len = snprintf(
            line,
            sizeof(line),
            "# Save-scoped amateur reputation history. Seed files are not modified.\r\n"
            "# TODO: add playoff-result memory offsets once verified.\r\n"
            "year,league_id,team_id,old_reputation,delta,new_reputation,wins,losses,ties,score,rank,source\r\n");
        ok = len > 0 && (size_t)len < sizeof(line)
            && WriteFile(file, line, (DWORD)len, &written, NULL)
            && written == (DWORD)len;
    }

    for (int i = 0; ok && i < row_count; i++) {
        const KboAmateurReputationUpdateRow* row = &rows[i];
        int32_t delta = (int32_t)row->new_reputation - (int32_t)row->old_reputation;
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%d,%u,%u,%u,%u,%d,%d,%s\r\n",
            year,
            row->league_id,
            row->team_id,
            (uint32_t)row->old_reputation,
            delta,
            (uint32_t)row->new_reputation,
            (uint32_t)row->wins,
            (uint32_t)row->losses,
            (uint32_t)row->ties,
            row->score,
            i + 1,
            source != NULL ? source : "");
        ok = len > 0 && (size_t)len < sizeof(line)
            && WriteFile(file, line, (DWORD)len, &written, NULL)
            && written == (DWORD)len;
    }

    CloseHandle(file);
    if (!ok) {
        append_logf("amateur reputation history write incomplete source=%s league=%u year=%u path=%s",
            source != NULL ? source : "", league_id, year, path);
        return 0;
    }
    append_logf("amateur reputation history appended source=%s league=%u year=%u rows=%d path=%s",
        source != NULL ? source : "", league_id, year, row_count, path);
    return 1;
}

static int kbo_update_amateur_reputation_for_league(uint32_t league_id, const char* source, uint32_t year)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }
    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    KboAmateurReputationUpdateRow rows[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
    int row_count = 0;
    for (int32_t i = 0; i < team_count && row_count < KBO_AMATEUR_ASSIGNMENT_TEAM_MAX; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint8_t old_rep = 70u;
        if (team_id == 0u || !kbo_find_amateur_team_reputation_by_memory_team(league_id, team, &old_rep)) {
            continue;
        }

        uint16_t wins = *(uint16_t*)(team + KBO_AMATEUR_TEAM_REGULAR_WINS_OFFSET);
        uint16_t losses = *(uint16_t*)(team + KBO_AMATEUR_TEAM_REGULAR_LOSSES_OFFSET);
        uint16_t ties = *(uint16_t*)(team + KBO_AMATEUR_TEAM_REGULAR_TIES_OFFSET);
        uint32_t games = (uint32_t)wins + (uint32_t)losses + (uint32_t)ties;
        if (games < KBO_AMATEUR_REPUTATION_UPDATE_MIN_GAMES || wins > 250u || losses > 250u || ties > 250u) {
            continue;
        }

        int32_t pct_score = (int32_t)(((uint32_t)wins * 2000u + (uint32_t)ties * 1000u) / games);
        rows[row_count++] = (KboAmateurReputationUpdateRow){
            league_id,
            team_id,
            old_rep,
            old_rep,
            wins,
            losses,
            ties,
            pct_score
        };
    }

    if (row_count < 10) {
        append_logf("amateur reputation update skipped source=%s league=%u year=%u reason=too_few_rows rows=%d",
            source != NULL ? source : "", league_id, year, row_count);
        return 0;
    }

    qsort(rows, (size_t)row_count, sizeof(rows[0]), kbo_compare_amateur_reputation_update_rows);
    for (int i = 0; i < row_count; i++) {
        int32_t delta = 0;
        if (rows[i].score >= 1600) {
            delta += 4;
        } else if (rows[i].score >= 1300) {
            delta += 2;
        } else if (rows[i].score >= 1100) {
            delta += 1;
        } else if (rows[i].score <= 500) {
            delta -= 4;
        } else if (rows[i].score <= 700) {
            delta -= 2;
        } else if (rows[i].score <= 900) {
            delta -= 1;
        }

        if (i < 4) {
            delta += 2;
        } else if (i < 10) {
            delta += 1;
        }
        if (i >= row_count - 4) {
            delta -= 2;
        } else if (i >= row_count - 10) {
            delta -= 1;
        }

        rows[i].new_reputation = kbo_amateur_reputation_clamp_for_league(
            league_id,
            (int32_t)rows[i].old_reputation + delta);
    }

    if (kbo_amateur_reputation_history_has_year(league_id, year)) {
        append_logf("amateur reputation update skipped source=%s league=%u year=%u reason=history_already_exists",
            source != NULL ? source : "", league_id, year);
        return row_count;
    }

    if (!kbo_append_amateur_reputation_history(league_id, rows, row_count, source, year)) {
        return 0;
    }

    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < row_count; i++) {
        kbo_amateur_reputation_add_seed(league_id, rows[i].team_id, "", "", "", rows[i].new_reputation);
    }
    kbo_unlock_amateur_reputation_seeds();
    kbo_invalidate_amateur_assignment_candidate_cache();

    int top = row_count < 3 ? row_count : 3;
    for (int i = 0; i < top; i++) {
        append_logf("amateur reputation update top source=%s league=%u year=%u rank=%d team=%u record=%u-%u-%u rep=%u->%u score=%d",
            source != NULL ? source : "", league_id, year, i + 1, rows[i].team_id,
            (uint32_t)rows[i].wins, (uint32_t)rows[i].losses, (uint32_t)rows[i].ties,
            (uint32_t)rows[i].old_reputation, (uint32_t)rows[i].new_reputation, rows[i].score);
    }
    append_logf("amateur reputation update summary source=%s league=%u year=%u rows=%d playoff=todo regular_offsets=w:0x%x,l:0x%x,t:0x%x",
        source != NULL ? source : "", league_id, year, row_count,
        KBO_AMATEUR_TEAM_REGULAR_WINS_OFFSET,
        KBO_AMATEUR_TEAM_REGULAR_LOSSES_OFFSET,
        KBO_AMATEUR_TEAM_REGULAR_TIES_OFFSET);
    return row_count;
}

static void kbo_update_amateur_reputation_from_team_records(const char* source)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day) || month != 1u || day != 1u) {
        return;
    }

    if (g_kbo_amateur_reputation_last_update_high_school_year != year) {
        int updated = kbo_update_amateur_reputation_for_league(KBO_HIGH_SCHOOL_LEAGUE_ID, source, year);
        if (updated > 0) {
            g_kbo_amateur_reputation_last_update_high_school_year = year;
        }
    }
    if (g_kbo_amateur_reputation_last_update_college_year != year) {
        int updated = kbo_update_amateur_reputation_for_league(KBO_COLLEGE_LEAGUE_ID, source, year);
        if (updated > 0) {
            g_kbo_amateur_reputation_last_update_college_year = year;
        }
    }
}

static uint32_t kbo_amateur_assignment_processed_hash_key(uint32_t player_id, uint32_t team_id)
{
    uint32_t hash = 2166136261u;
    hash = (hash ^ player_id) * 16777619u;
    hash = (hash ^ team_id) * 16777619u;
    hash ^= hash >> 16;
    return hash;
}

static void kbo_amateur_assignment_clear_processed_cache(void)
{
    memset(g_kbo_amateur_assignment_processed, 0, sizeof(g_kbo_amateur_assignment_processed));
    memset(g_kbo_amateur_assignment_processed_hash, 0, sizeof(g_kbo_amateur_assignment_processed_hash));
    InterlockedExchange(&g_kbo_amateur_assignment_processed_count, 0);
    InterlockedExchange(&g_kbo_amateur_assignment_processed_hash_count, 0);
}

static int kbo_amateur_assignment_already_processed(uint32_t player_id, uint32_t team_id)
{
    if (player_id == 0u || team_id == 0u) {
        return 0;
    }

    uint32_t start = kbo_amateur_assignment_processed_hash_key(player_id, team_id)
        & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK;
    for (uint32_t probe = 0; probe < KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX; probe++) {
        KboAmateurAssignmentProcessed* slot =
            &g_kbo_amateur_assignment_processed_hash[(start + probe) & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK];
        if (slot->player_id == 0u) {
            return 0;
        }
        if (slot->player_id == player_id && slot->team_id == team_id) {
            return 1;
        }
    }
    return 1;
}

static void kbo_amateur_assignment_mark_processed(uint32_t player_id, uint32_t team_id)
{
    if (player_id == 0u || team_id == 0u) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_amateur_assignment_processed_hash_count, 0, 0)
            > (LONG)(KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX * 3 / 4)) {
        kbo_amateur_assignment_clear_processed_cache();
    }

    uint32_t start = kbo_amateur_assignment_processed_hash_key(player_id, team_id)
        & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK;
    for (uint32_t probe = 0; probe < KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX; probe++) {
        KboAmateurAssignmentProcessed* hash_slot =
            &g_kbo_amateur_assignment_processed_hash[(start + probe) & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK];
        if (hash_slot->player_id == player_id && hash_slot->team_id == team_id) {
            return;
        }
        if (hash_slot->player_id == 0u) {
            hash_slot->player_id = player_id;
            hash_slot->team_id = team_id;
            InterlockedIncrement(&g_kbo_amateur_assignment_processed_hash_count);
            break;
        }
    }

    LONG slot = InterlockedIncrement(&g_kbo_amateur_assignment_processed_count) - 1;
    if (slot < 0) {
        return;
    }
    if (slot >= KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX) {
        kbo_amateur_assignment_clear_processed_cache();
        slot = InterlockedIncrement(&g_kbo_amateur_assignment_processed_count) - 1;
    }
    g_kbo_amateur_assignment_processed[slot].player_id = player_id;
    g_kbo_amateur_assignment_processed[slot].team_id = team_id;
}

static int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age);

static void kbo_read_amateur_quality_fields(
    uint8_t* player,
    int16_t* out_overall,
    int16_t* out_talent,
    int16_t* out_ratings,
    int16_t* out_career)
{
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_talent != NULL) { *out_talent = 0; }
    if (out_ratings != NULL) { *out_ratings = 0; }
    if (out_career != NULL) { *out_career = 0; }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET + sizeof(int16_t))) {
        return;
    }
    if (out_overall != NULL) { *out_overall = *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET); }
    if (out_talent != NULL) { *out_talent = *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET); }
    if (out_ratings != NULL) { *out_ratings = *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET); }
    if (out_career != NULL) { *out_career = *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET); }
}

static int32_t kbo_amateur_quality_score(uint8_t* player)
{
    int16_t overall = 0;
    int16_t talent = 0;
    int16_t ratings = 0;
    int16_t career = 0;
    kbo_read_amateur_quality_fields(player, &overall, &talent, &ratings, &career);
    return overall + talent + ratings + career;
}

static int32_t kbo_amateur_assignment_target_reputation(uint32_t league_id, int32_t quality_score)
{
    int32_t target = 50;
    if (quality_score >= 2300) {
        target = 92;
    } else if (quality_score >= 1800) {
        target = 84;
    } else if (quality_score >= 1350) {
        target = 72;
    } else if (quality_score >= 950) {
        target = 58;
    } else if (quality_score >= 600) {
        target = 45;
    } else {
        target = 35;
    }

    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        target -= 10;
    }
    if (target < 25) {
        target = 25;
    } else if (target > 95) {
        target = 95;
    }
    return target;
}

static uint32_t kbo_amateur_assignment_pick_hash(uint32_t player_id, uint32_t league_id, int32_t quality_score, int32_t target)
{
    uint32_t hash = 2166136261u;
    hash = (hash ^ player_id) * 16777619u;
    hash = (hash ^ league_id) * 16777619u;
    hash = (hash ^ (uint32_t)quality_score) * 16777619u;
    hash = (hash ^ (uint32_t)target) * 16777619u;
    hash = (hash ^ 0x9e3779b9u) * 16777619u;
    return hash;
}

static int kbo_amateur_assignment_target_rejected(uint32_t league_id, uint32_t team_id)
{
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }
    LONG count = InterlockedCompareExchange(&g_kbo_amateur_assignment_rejected_target_count, 0, 0);
    if (count > KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX) {
        count = KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX;
    }
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_amateur_assignment_rejected_targets[i].player_id == league_id
                && g_kbo_amateur_assignment_rejected_targets[i].team_id == team_id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_amateur_assignment_mark_rejected_target(uint32_t league_id, uint32_t team_id)
{
    if (league_id == 0u || team_id == 0u || kbo_amateur_assignment_target_rejected(league_id, team_id)) {
        return;
    }
    LONG slot = InterlockedIncrement(&g_kbo_amateur_assignment_rejected_target_count) - 1;
    if (slot < 0 || slot >= KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX) {
        return;
    }
    g_kbo_amateur_assignment_rejected_targets[slot].player_id = league_id;
    g_kbo_amateur_assignment_rejected_targets[slot].team_id = team_id;
}

static int kbo_amateur_assignment_team_tier(uint32_t league_id, uint8_t reputation)
{
    int32_t rep = (int32_t)reputation;
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        if (rep >= 68) { return 5; }
        if (rep >= 55) { return 4; }
        if (rep >= 42) { return 3; }
        if (rep >= 28) { return 2; }
        if (rep >= 15) { return 1; }
        return 0;
    }
    if (rep >= 90) { return 5; }
    if (rep >= 80) { return 4; }
    if (rep >= 68) { return 3; }
    if (rep >= 56) { return 2; }
    if (rep >= 48) { return 1; }
    return 0;
}

static int kbo_amateur_assignment_player_tier(uint32_t league_id, int32_t quality_score)
{
    (void)league_id;
    int tier = 0;
    if (quality_score >= 2300) {
        tier = 5;
    } else if (quality_score >= 1800) {
        tier = 4;
    } else if (quality_score >= 1350) {
        tier = 3;
    } else if (quality_score >= 950) {
        tier = 2;
    } else if (quality_score >= 600) {
        tier = 1;
    } else {
        tier = 0;
    }
    return tier;
}

static int kbo_amateur_assignment_tier_allowed(int player_tier, int team_tier)
{
    return player_tier == team_tier;
}

static int kbo_amateur_assignment_effective_player_tier(int player_tier, int max_team_tier)
{
    if (max_team_tier < 0) {
        return player_tier;
    }
    if (player_tier > max_team_tier) {
        return max_team_tier;
    }
    return player_tier;
}

static int kbo_amateur_assignment_debug_csv_empty(HANDLE file)
{
    LARGE_INTEGER size;
    if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &size)) {
        return 1;
    }
    return size.QuadPart == 0;
}

static void kbo_amateur_assignment_debug_write_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

static void kbo_amateur_assignment_debug_write_csv_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    kbo_amateur_assignment_debug_write_text(file, "\"");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                kbo_amateur_assignment_debug_write_text(file, "\"\"");
            } else {
                char ch[2] = { *p, '\0' };
                kbo_amateur_assignment_debug_write_text(file, ch);
            }
        }
    }
    kbo_amateur_assignment_debug_write_text(file, "\"");
}

static void kbo_amateur_assignment_append_debug_csv(
    const char* phase,
    const char* source,
    uint32_t player_id,
    uint32_t league_id,
    int16_t age,
    int32_t quality_score,
    int player_tier,
    uint32_t from_team_id,
    uint8_t from_reputation,
    int from_tier,
    int32_t from_player_count,
    uint32_t to_team_id,
    uint8_t to_reputation,
    int to_tier,
    int32_t to_player_count,
    int32_t target_player_count,
    uint8_t target_reputation,
    int original_result,
    uint32_t after_team_id,
    uint32_t after_league_id)
{
    if (!read_kbo_localappdata_flag_file("enable_amateur_assignment_debug_csv.txt")) {
        return;
    }
    if (strcmp(phase != NULL ? phase : "", "original_success") == 0) {
        static volatile LONG success_csv_count = 0;
        if (InterlockedIncrement(&success_csv_count) > 300) {
            return;
        }
    }
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_file("amateur_assignment_debug.csv", path, sizeof(path))) {
        return;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        static volatile LONG open_log_count = 0;
        if (InterlockedIncrement(&open_log_count) <= 5) {
            append_logf("amateur assignment debug csv open failed path=%s gle=%lu", path, GetLastError());
        }
        return;
    }

    if (kbo_amateur_assignment_debug_csv_empty(file)) {
        kbo_amateur_assignment_debug_write_text(
            file,
            "year,month,day,phase,source,player_id,league_id,age,quality_score,player_tier,"
            "from_team_id,from_reputation,from_tier,from_player_count,"
            "to_team_id,to_reputation,to_tier,to_player_count,target_player_count,target_reputation,"
            "original_result,after_team_id,after_league_id\r\n");
    }

    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    kbo_current_date_is_valid(&year, &month, &day);

    char line[1024] = {0};
    snprintf(
        line,
        sizeof(line),
        "%u,%u,%u,",
        year,
        month,
        day);
    kbo_amateur_assignment_debug_write_text(file, line);
    kbo_amateur_assignment_debug_write_csv_text(file, phase != NULL ? phase : "");
    kbo_amateur_assignment_debug_write_text(file, ",");
    kbo_amateur_assignment_debug_write_csv_text(file, source != NULL ? source : "");
    snprintf(
        line,
        sizeof(line),
        ",%u,%u,%d,%d,%d,%u,%u,%d,%d,%u,%u,%d,%d,%d,%u,%d,%u,%u\r\n",
        player_id,
        league_id,
        (int)age,
        quality_score,
        player_tier,
        from_team_id,
        (uint32_t)from_reputation,
        from_tier,
        from_player_count,
        to_team_id,
        (uint32_t)to_reputation,
        to_tier,
        to_player_count,
        target_player_count,
        (uint32_t)target_reputation,
        original_result,
        after_team_id,
        after_league_id);
    kbo_amateur_assignment_debug_write_text(file, line);
    CloseHandle(file);
}

static uint32_t kbo_amateur_assignment_candidate_weight(
    uint32_t league_id,
    int32_t quality_score,
    int assignment_tier,
    int32_t target,
    uint8_t reputation,
    int32_t player_count,
    int32_t target_player_count)
{
    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);
    if (assignment_tier >= 0) {
        player_tier = assignment_tier;
    }
    int team_tier = kbo_amateur_assignment_team_tier(league_id, reputation);
    if (!kbo_amateur_assignment_tier_allowed(player_tier, team_tier)) {
        return 0u;
    }
    if (target_player_count >= 0 && player_count > target_player_count) {
        return 0u;
    }

    int32_t distance = abs((int32_t)reputation - target);
    int32_t base = 72 - (distance * 3);
    if (base < 12) {
        base = 12;
    }
    uint32_t weight = (uint32_t)base;
    if (weight == 0u) {
        weight = 1u;
    }
    return weight;
}

static int kbo_amateur_assignment_collect_candidates(
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int max_candidates)
{
    if (league_id == 0u || candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int found = 0;
    for (int32_t i = 0; i < team_count && found < max_candidates; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint8_t reputation = 70u;
        if (team_id == 0u || !kbo_find_amateur_team_reputation_by_memory_team(league_id, team, &reputation)) {
            continue;
        }

        candidates[found].team = team;
        candidates[found].team_id = team_id;
        candidates[found].reputation = reputation;
        candidates[found].player_count = 0;
        found++;
    }
    return found;
}

static void kbo_amateur_assignment_refresh_player_counts(
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int candidate_count)
{
    if (league_id == 0u || candidates == NULL || candidate_count <= 0) {
        return;
    }

    for (int i = 0; i < candidate_count; i++) {
        candidates[i].player_count = 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (!kbo_amateur_player_age_eligible(league_id, age)) {
            continue;
        }
        uint32_t team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (team_id == 0u) {
            team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        }
        if (team_id == 0u) {
            continue;
        }
        for (int c = 0; c < candidate_count; c++) {
            if (candidates[c].team_id == team_id) {
                candidates[c].player_count++;
                break;
            }
        }
    }
}

static void kbo_amateur_assignment_note_player_count_delta(uint32_t league_id, uint32_t team_id, int32_t delta)
{
    if (team_id == 0u || delta == 0) {
        return;
    }
    int* count_ptr = kbo_amateur_assignment_count_ptr_for_league(league_id);
    KboAmateurAssignmentCandidate* candidates = kbo_amateur_assignment_cache_for_league(league_id);
    if (count_ptr == NULL || candidates == NULL || *count_ptr <= 0) {
        return;
    }

    kbo_lock_amateur_assignment_candidates();
    int count = *count_ptr;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            candidates[i].player_count += delta;
            if (candidates[i].player_count < 0) {
                candidates[i].player_count = 0;
            }
            break;
        }
    }
    kbo_unlock_amateur_assignment_candidates();
}

static int kbo_amateur_assignment_get_cached_candidates(
    uint32_t league_id,
    KboAmateurAssignmentCandidate** out_candidates)
{
    if (out_candidates != NULL) {
        *out_candidates = NULL;
    }
    if (league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID) {
        return 0;
    }

    KboAmateurAssignmentCandidate* cache = kbo_amateur_assignment_cache_for_league(league_id);
    int* count_ptr = kbo_amateur_assignment_count_ptr_for_league(league_id);
    if (cache == NULL || count_ptr == NULL) {
        return 0;
    }

    if (*count_ptr >= 0) {
        if (out_candidates != NULL) {
            *out_candidates = cache;
        }
        return *count_ptr;
    }

    kbo_lock_amateur_assignment_candidates();
    if (*count_ptr < 0) {
        memset(cache, 0, sizeof(KboAmateurAssignmentCandidate) * KBO_AMATEUR_ASSIGNMENT_TEAM_MAX);
        *count_ptr = kbo_amateur_assignment_collect_candidates(
            league_id,
            cache,
            KBO_AMATEUR_ASSIGNMENT_TEAM_MAX);
        kbo_amateur_assignment_refresh_player_counts(league_id, cache, *count_ptr);
        append_logf(
            "amateur assignment candidate cache built league=%u teams=%d counts=refreshed",
            league_id,
            *count_ptr);
    }
    int count = *count_ptr;
    kbo_unlock_amateur_assignment_candidates();

    if (out_candidates != NULL) {
        *out_candidates = cache;
    }
    return count;
}

static int kbo_amateur_assignment_find_candidate_info(
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* out_reputation,
    int* out_tier,
    int32_t* out_player_count)
{
    if (out_reputation != NULL) { *out_reputation = 0u; }
    if (out_tier != NULL) { *out_tier = -1; }
    if (out_player_count != NULL) { *out_player_count = -1; }
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 0 || candidates == NULL) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            if (out_reputation != NULL) {
                *out_reputation = candidates[i].reputation;
            }
            if (out_tier != NULL) {
                *out_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
            }
            if (out_player_count != NULL) {
                *out_player_count = candidates[i].player_count;
            }
            return 1;
        }
    }
    return 0;
}

static uint8_t* kbo_choose_amateur_assignment_team(
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    uint8_t* out_target_reputation)
{
    if (out_target_reputation != NULL) {
        *out_target_reputation = 0u;
    }
    if (player == NULL || league_id == 0u || current_team_id == 0u || quality_score <= 0) {
        return NULL;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1) {
        return NULL;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int32_t target = kbo_amateur_assignment_target_reputation(league_id, quality_score);
    if (out_target_reputation != NULL) {
        *out_target_reputation = (uint8_t)target;
    }

    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);
    if (player_tier > 0 && target <= (int32_t)current_reputation) {
        return NULL;
    }
    int max_team_tier = -1;
    for (int i = 0; i < count; i++) {
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        int team_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
        if (team_tier > max_team_tier) {
            max_team_tier = team_tier;
        }
    }
    int assignment_tier = kbo_amateur_assignment_effective_player_tier(player_tier, max_team_tier);

    int32_t current_player_count = 0;
    int32_t target_player_count = 0x7fffffff;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            current_player_count = candidates[i].player_count;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        if (kbo_amateur_assignment_candidate_weight(
                league_id,
                quality_score,
                assignment_tier,
                target,
                candidates[i].reputation,
                candidates[i].player_count,
                -1) == 0u) {
            continue;
        }
        if (candidates[i].player_count < target_player_count) {
            target_player_count = candidates[i].player_count;
        }
    }
    if (target_player_count == 0x7fffffff) {
        return NULL;
    }

    uint32_t total_weight = 0u;
    uint32_t current_weight = kbo_amateur_assignment_candidate_weight(
        league_id,
        quality_score,
        assignment_tier,
        target,
        current_reputation,
        current_player_count,
        target_player_count);
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            continue;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        uint32_t weight = kbo_amateur_assignment_candidate_weight(
            league_id,
            quality_score,
            assignment_tier,
            target,
            candidates[i].reputation,
            candidates[i].player_count,
            target_player_count);
        if (weight == 0u) {
            continue;
        }
        total_weight += weight;
    }

    if (total_weight == 0u) {
        return NULL;
    }

    if (current_weight > 0u) {
        return NULL;
    }

    uint32_t pick = kbo_amateur_assignment_pick_hash(player_id, league_id, quality_score, target) % total_weight;
    uint32_t seen = 0u;
    int fallback_index = -1;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            continue;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        uint32_t weight = kbo_amateur_assignment_candidate_weight(
            league_id,
            quality_score,
            assignment_tier,
            target,
            candidates[i].reputation,
            candidates[i].player_count,
            target_player_count);
        if (weight == 0u) {
            continue;
        }
        fallback_index = i;
        if (pick < seen + weight) {
            return candidates[i].team;
        }
        seen += weight;
    }

    return fallback_index >= 0 ? candidates[fallback_index].team : NULL;
}

static uintptr_t kbo_amateur_team_add_player_reroute_before_original(uintptr_t team_ptr, uintptr_t player_ptr, const char* source)
{
    if (read_kbo_localappdata_flag_file("disable_amateur_assignment_reroute.txt")
            || team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return team_ptr;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = kbo_resolve_amateur_assignment_league_id_for_team_ptr(team);
    if (team_league_id == 0u) {
        return team_ptr;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if (player_id == 0u || !kbo_amateur_player_age_eligible(team_league_id, age)
            || kbo_amateur_assignment_already_processed(player_id, team_id)) {
        return team_ptr;
    }

    uint8_t current_reputation = 70u;
    if (!kbo_find_amateur_team_reputation_by_memory_team(team_league_id, team, &current_reputation)) {
        return team_ptr;
    }

    if (team_league_id == KBO_COLLEGE_LEAGUE_ID) {
        int32_t current_player_count = -1;
        kbo_amateur_assignment_find_candidate_info(
            team_league_id,
            team_id,
            NULL,
            NULL,
            &current_player_count);
        if (current_player_count >= 0 && current_player_count < KBO_COLLEGE_ASSIGNMENT_SOURCE_MIN_PLAYERS) {
            return team_ptr;
        }
    }

    int32_t quality_score = kbo_amateur_quality_score(player);
    uint8_t target_reputation = 0u;
    uint8_t* target_team = kbo_choose_amateur_assignment_team(
        player,
        team_league_id,
        team_id,
        current_reputation,
        quality_score,
        &target_reputation);
    if (target_team == NULL || target_team == team) {
        int player_tier = kbo_amateur_assignment_player_tier(team_league_id, quality_score);
        int from_tier = kbo_amateur_assignment_team_tier(team_league_id, current_reputation);
        int32_t from_player_count = -1;
        kbo_amateur_assignment_find_candidate_info(
            team_league_id,
            team_id,
            NULL,
            NULL,
            &from_player_count);
        kbo_amateur_assignment_append_debug_csv(
            target_team == team ? "pre_keep_same_tier_slot" : "pre_keep_no_candidate",
            source,
            player_id,
            team_league_id,
            age,
            quality_score,
            player_tier,
            team_id,
            current_reputation,
            from_tier,
            from_player_count,
            team_id,
            current_reputation,
            from_tier,
            from_player_count,
            from_player_count,
            target_reputation,
            -1,
            0u,
            0u);
        return team_ptr;
    }

    uint32_t target_team_id = *(uint32_t*)(target_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint8_t actual_target_reputation = 70u;
    kbo_find_amateur_team_reputation_by_memory_team(team_league_id, target_team, &actual_target_reputation);
    int player_tier = kbo_amateur_assignment_player_tier(team_league_id, quality_score);
    int from_tier = kbo_amateur_assignment_team_tier(team_league_id, current_reputation);
    int to_tier = kbo_amateur_assignment_team_tier(team_league_id, actual_target_reputation);
    int32_t from_player_count = -1;
    int32_t to_player_count = -1;
    kbo_amateur_assignment_find_candidate_info(
        team_league_id,
        team_id,
        NULL,
        NULL,
        &from_player_count);
    kbo_amateur_assignment_find_candidate_info(
        team_league_id,
        target_team_id,
        NULL,
        NULL,
        &to_player_count);
    int32_t target_player_count = to_player_count;

    kbo_amateur_assignment_append_debug_csv(
        "pre_reroute_decision",
        source,
        player_id,
        team_league_id,
        age,
        quality_score,
        player_tier,
        team_id,
        current_reputation,
        from_tier,
        from_player_count,
        target_team_id,
        actual_target_reputation,
        to_tier,
        to_player_count,
        target_player_count,
        target_reputation,
        -1,
        0u,
        0u);

    static volatile LONG reroute_before_log_count = 0;
    LONG slot = InterlockedIncrement(&reroute_before_log_count);
    int verbose_assignment_log = read_kbo_localappdata_flag_file("enable_amateur_assignment_verbose_log.txt");
    if (verbose_assignment_log || slot <= 30) {
        append_logf(
            "amateur assignment pre-reroute team-add source=%s player=%u league=%u age=%d score=%d target_rep=%u team=%u(rep=%u)->%u(rep=%u)",
            source != NULL ? source : "",
            player_id,
            team_league_id,
            (int)age,
            quality_score,
            (uint32_t)target_reputation,
            team_id,
            (uint32_t)current_reputation,
            target_team_id,
            (uint32_t)actual_target_reputation);
    } else if (slot == 31) {
        append_log_line("amateur assignment pre-reroute log suppressed after 30 players; create enable_amateur_assignment_verbose_log.txt for full logging");
    }

    return (uintptr_t)target_team;
}

static void kbo_amateur_team_add_player_note_original_success(uintptr_t team_ptr, uintptr_t player_ptr, const char* source, int original_result)
{
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }
    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = kbo_resolve_amateur_assignment_league_id_for_team_ptr(team);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_league_id == 0u
            || team_id == 0u
            || player_id == 0u
            || kbo_amateur_assignment_already_processed(player_id, team_id)) {
        return;
    }
    kbo_amateur_assignment_mark_processed(player_id, team_id);
    kbo_amateur_assignment_note_player_count_delta(team_league_id, team_id, 1);

    if (!read_kbo_localappdata_flag_file("enable_amateur_assignment_debug_csv.txt")) {
        return;
    }

    uint8_t reputation = 0u;
    int team_tier = -1;
    int32_t player_count = -1;
    kbo_amateur_assignment_find_candidate_info(
        team_league_id,
        team_id,
        &reputation,
        &team_tier,
        &player_count);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    int32_t quality_score = kbo_amateur_quality_score(player);
    int player_tier = kbo_amateur_assignment_player_tier(team_league_id, quality_score);
    uint32_t after_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    kbo_amateur_assignment_append_debug_csv(
        "original_success",
        source,
        player_id,
        team_league_id,
        age,
        quality_score,
        player_tier,
        team_id,
        reputation,
        team_tier,
        player_count,
        team_id,
        reputation,
        team_tier,
        player_count,
        player_count,
        reputation,
        original_result,
        after_team_id,
        after_league_id);
}

static int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age)
{
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return age >= 17 && age <= 25;
    }
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return age >= 14 && age <= 18;
    }
    return 0;
}
