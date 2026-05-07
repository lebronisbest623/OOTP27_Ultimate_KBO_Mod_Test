/* High-school freshman quality shaping from team reputation seed. */

#define KBO_HIGH_SCHOOL_LEAGUE_ID 203u
#define KBO_HIGH_SCHOOL_REPUTATION_SEED_MAX 256
#define KBO_HIGH_SCHOOL_FRESHMAN_PROCESSED_MAX 16384

typedef struct KboHighSchoolReputationSeed {
    uint32_t team_id;
    uint8_t reputation;
} KboHighSchoolReputationSeed;

typedef struct KboHighSchoolFreshmanProcessed {
    uint32_t player_id;
    uint32_t team_id;
} KboHighSchoolFreshmanProcessed;

static KboHighSchoolReputationSeed g_kbo_high_school_reputation_seeds[KBO_HIGH_SCHOOL_REPUTATION_SEED_MAX];
static int g_kbo_high_school_reputation_seed_count = 0;
static LONG g_kbo_high_school_reputation_seed_lock = 0;
static LONG g_kbo_high_school_reputation_seed_loaded = 0;
static char g_kbo_high_school_reputation_seed_loaded_path[MAX_PATH] = {0};
static KboHighSchoolFreshmanProcessed g_kbo_high_school_freshman_processed[KBO_HIGH_SCHOOL_FRESHMAN_PROCESSED_MAX];
static LONG g_kbo_high_school_freshman_processed_count = 0;

static void kbo_lock_high_school_reputation_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_high_school_reputation_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_high_school_reputation_seeds(void)
{
    InterlockedExchange(&g_kbo_high_school_reputation_seed_lock, 0);
}

static void kbo_high_school_reputation_trim_cell(char* value)
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

static void kbo_high_school_reputation_read_cell(const char** cursor, char* out, size_t out_size)
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
    kbo_high_school_reputation_trim_cell(out);
    *cursor = p;
}

static uint32_t kbo_high_school_reputation_parse_u32(const char* text)
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

static int kbo_get_high_school_reputation_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    if (kbo_get_save_scoped_data_file("high_school_reputation_seed.csv", out, out_size)) {
        DWORD attrs = GetFileAttributesA(out);
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return 1;
        }
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\high_school_reputation_seed.csv", local_app_data);
    return out[0] != '\0';
}

static int kbo_read_high_school_reputation_seed_file(const char* path, char** out_buffer, DWORD* out_size)
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

static int kbo_high_school_reputation_add_seed(uint32_t team_id, uint32_t reputation)
{
    if (team_id == 0u) {
        return 0;
    }
    if (reputation < 1u) {
        reputation = 1u;
    } else if (reputation > 100u) {
        reputation = 100u;
    }

    for (int i = 0; i < g_kbo_high_school_reputation_seed_count; i++) {
        if (g_kbo_high_school_reputation_seeds[i].team_id == team_id) {
            g_kbo_high_school_reputation_seeds[i].reputation = (uint8_t)reputation;
            return 1;
        }
    }
    if (g_kbo_high_school_reputation_seed_count >= KBO_HIGH_SCHOOL_REPUTATION_SEED_MAX) {
        return 0;
    }
    g_kbo_high_school_reputation_seeds[g_kbo_high_school_reputation_seed_count].team_id = team_id;
    g_kbo_high_school_reputation_seeds[g_kbo_high_school_reputation_seed_count].reputation = (uint8_t)reputation;
    g_kbo_high_school_reputation_seed_count++;
    return 1;
}

static void kbo_ensure_high_school_reputation_seeds_loaded(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_high_school_reputation_seed_path(path, sizeof(path))) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_high_school_reputation_seed_loaded, 1, 1) == 1
            && strcmp(path, g_kbo_high_school_reputation_seed_loaded_path) == 0) {
        return;
    }

    char* buffer = NULL;
    DWORD size = 0;
    if (!kbo_read_high_school_reputation_seed_file(path, &buffer, &size)) {
        append_logf("high school reputation seed load skipped path=%s reason=read_failed", path);
        return;
    }

    kbo_lock_high_school_reputation_seeds();
    memset(g_kbo_high_school_reputation_seeds, 0, sizeof(g_kbo_high_school_reputation_seeds));
    g_kbo_high_school_reputation_seed_count = 0;

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
        uint32_t team_id = 0u;
        uint32_t reputation = 0u;
        for (int col = 0; col <= 9; col++) {
            kbo_high_school_reputation_read_cell(&cell_cursor, cell, sizeof(cell));
            if (col == 5) {
                team_id = kbo_high_school_reputation_parse_u32(cell);
            } else if (col == 9) {
                reputation = kbo_high_school_reputation_parse_u32(cell);
            }
        }
        kbo_high_school_reputation_add_seed(team_id, reputation);
    }

    strncpy(g_kbo_high_school_reputation_seed_loaded_path, path, sizeof(g_kbo_high_school_reputation_seed_loaded_path) - 1u);
    g_kbo_high_school_reputation_seed_loaded_path[sizeof(g_kbo_high_school_reputation_seed_loaded_path) - 1u] = '\0';
    InterlockedExchange(&g_kbo_high_school_reputation_seed_loaded, 1);
    int count = g_kbo_high_school_reputation_seed_count;
    kbo_unlock_high_school_reputation_seeds();
    HeapFree(GetProcessHeap(), 0, buffer);
    append_logf("high school reputation seed loaded count=%d path=%s", count, path);
}

static int kbo_find_high_school_reputation(uint32_t team_id, uint8_t* out_reputation)
{
    if (out_reputation != NULL) {
        *out_reputation = 70u;
    }
    if (team_id == 0u) {
        return 0;
    }
    kbo_ensure_high_school_reputation_seeds_loaded();
    kbo_lock_high_school_reputation_seeds();
    for (int i = 0; i < g_kbo_high_school_reputation_seed_count; i++) {
        if (g_kbo_high_school_reputation_seeds[i].team_id == team_id) {
            if (out_reputation != NULL) {
                *out_reputation = g_kbo_high_school_reputation_seeds[i].reputation;
            }
            kbo_unlock_high_school_reputation_seeds();
            return 1;
        }
    }
    kbo_unlock_high_school_reputation_seeds();
    return 0;
}

static int kbo_high_school_freshman_already_processed(uint32_t player_id, uint32_t team_id)
{
    LONG count = g_kbo_high_school_freshman_processed_count;
    if (count > KBO_HIGH_SCHOOL_FRESHMAN_PROCESSED_MAX) {
        count = KBO_HIGH_SCHOOL_FRESHMAN_PROCESSED_MAX;
    }
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_high_school_freshman_processed[i].player_id == player_id
                && g_kbo_high_school_freshman_processed[i].team_id == team_id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_high_school_freshman_mark_processed(uint32_t player_id, uint32_t team_id)
{
    LONG slot = InterlockedIncrement(&g_kbo_high_school_freshman_processed_count) - 1;
    if (slot < 0) {
        return;
    }
    if (slot >= KBO_HIGH_SCHOOL_FRESHMAN_PROCESSED_MAX) {
        InterlockedExchange(&g_kbo_high_school_freshman_processed_count, 0);
        slot = InterlockedIncrement(&g_kbo_high_school_freshman_processed_count) - 1;
    }
    g_kbo_high_school_freshman_processed[slot].player_id = player_id;
    g_kbo_high_school_freshman_processed[slot].team_id = team_id;
}

static int16_t kbo_high_school_scale_i16(int16_t value, uint32_t scale_percent)
{
    if (value <= 0 || scale_percent == 100u) {
        return value;
    }
    int32_t scaled = ((int32_t)value * (int32_t)scale_percent + 50) / 100;
    if (scaled < 1) {
        scaled = 1;
    }
    if (scaled > 32767) {
        scaled = 32767;
    }
    return (int16_t)scaled;
}

static int kbo_apply_high_school_reputation_quality(uint8_t* player, uint8_t reputation)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    uint32_t scale_percent = 65u + ((uint32_t)reputation / 2u);
    if (scale_percent < 70u) {
        scale_percent = 70u;
    } else if (scale_percent > 125u) {
        scale_percent = 125u;
    }

    int16_t old_overall = *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t old_talent = *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t old_ratings = *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t old_career = *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET);

    *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET) = kbo_high_school_scale_i16(old_overall, scale_percent);
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET) = kbo_high_school_scale_i16(old_talent, scale_percent);
    *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET) = kbo_high_school_scale_i16(old_ratings, scale_percent);
    *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET) = kbo_high_school_scale_i16(old_career, scale_percent);
    return scale_percent != 100u
        && (old_overall > 0 || old_talent > 0 || old_ratings > 0 || old_career > 0);
}

static int32_t kbo_high_school_quality_score(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET + sizeof(int16_t))) {
        return 0;
    }
    int32_t overall = *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    return overall + talent + ratings + career;
}

static void kbo_high_school_team_add_player_prepare_quality(uintptr_t team_ptr, uintptr_t player_ptr)
{
    if (read_kbo_localappdata_flag_file("disable_high_school_freshman_quality_shaping.txt")) {
        return;
    }
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }
    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id != KBO_HIGH_SCHOOL_LEAGUE_ID || team_id == 0u) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if (player_id == 0u || current_team_id == team_id || age < 14 || age > 18) {
        return;
    }
    if (kbo_high_school_freshman_already_processed(player_id, team_id)) {
        return;
    }

    uint8_t reputation = 70u;
    if (!kbo_find_high_school_reputation(team_id, &reputation)) {
        return;
    }
    int32_t before_score = kbo_high_school_quality_score(player);
    int changed = kbo_apply_high_school_reputation_quality(player, reputation);
    int32_t after_score = kbo_high_school_quality_score(player);
    kbo_high_school_freshman_mark_processed(player_id, team_id);

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 300) {
        append_logf(
            "high school freshman quality shaped at team-add player=%u team=%u age=%d reputation=%u score=%d->%d changed=%d",
            player_id,
            team_id,
            (int)age,
            (uint32_t)reputation,
            before_score,
            after_score,
            changed);
    } else if (slot == 301) {
        append_log_line("high school freshman quality team-add log suppressed after 300 players");
    }
}
