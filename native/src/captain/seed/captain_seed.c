#include "../internal/captain_selection_internal.h"

static int kbo_captain_seed_source_rank(const char* source)
{
    return source != NULL && _stricmp(source, "save_seed") == 0 ? 2 : 1;
}

typedef struct KboCaptainDisplayCache {
    uint32_t season;
    uint32_t league_id;
    uint32_t team_id;
    uint32_t player_id;
    int found;
    char loaded_key[MAX_PATH * 6];
    char player_name[128];
    char source[24];
} KboCaptainDisplayCache;

static volatile LONG g_kbo_captain_display_cache_lock = 0;
static KboCaptainDisplayCache g_kbo_captain_display_cache;

void kbo_lock_captain_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_captain_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_unlock_captain_seeds(void)
{
    InterlockedExchange(&g_kbo_captain_seed_lock, 0);
}

static void kbo_lock_captain_display_cache(void)
{
    while (InterlockedCompareExchange(&g_kbo_captain_display_cache_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_captain_display_cache(void)
{
    InterlockedExchange(&g_kbo_captain_display_cache_lock, 0);
}

int kbo_get_save_captain_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("captain_seed.csv", out, out_size);
}

int kbo_get_global_captain_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\captain_seed.csv", local_app_data);
    return out[0] != '\0';
}

static void kbo_captain_seed_file_loaded_key_component(const char* path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        snprintf(out, out_size, "none");
        return;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        snprintf(
            out,
            out_size,
            "%s:csv:%08lx%08lx:%08lx%08lx",
            path,
            (unsigned long)attrs.ftLastWriteTime.dwHighDateTime,
            (unsigned long)attrs.ftLastWriteTime.dwLowDateTime,
            (unsigned long)attrs.nFileSizeHigh,
            (unsigned long)attrs.nFileSizeLow);
        return;
    }

    snprintf(out, out_size, "%s:missing", path);
}

static int kbo_add_captain_seed_locked(const KboCaptainSeed* seed)
{
    if (seed == NULL
            || (seed->team_id == 0u && seed->team_code[0] == '\0')
            || (seed->player_id == 0u && seed->player_key[0] == '\0')) {
        return 0;
    }

    if (g_kbo_captain_seed_count >= KBO_CAPTAIN_SEED_MAX) {
        return 0;
    }
    g_kbo_captain_seeds[g_kbo_captain_seed_count++] = *seed;
    return 1;
}

static int kbo_captain_seed_line_parse_failure_is_reportable(char* line)
{
    if (line == NULL) {
        return 0;
    }

    char* check = line;
    while (*check == ' ' || *check == '\t') {
        check++;
    }
    if (*check == '\0' || *check == '#' || *check == ';') {
        return 0;
    }

    char first_field[64] = {0};
    size_t i = 0;
    while (check[i] != '\0' && check[i] != ',' && i + 1u < sizeof(first_field)) {
        first_field[i] = check[i];
        i++;
    }
    kbo_captain_trim_csv_token_in_place(first_field);
    return _stricmp(first_field, "season") != 0
        && _stricmp(first_field, "team_id") != 0
        && _stricmp(first_field, "team_code") != 0;
}

int kbo_import_captain_seed_file_locked(const char* path, const char* source)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 65536u) {
        CloseHandle(file);
        return 0;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int imported = 0;
    int parse_failed = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[512] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            KboCaptainSeed seed;
            if (kbo_parse_captain_seed_line(line, &seed)) {
                seed.source_rank = (uint8_t)kbo_captain_seed_source_rank(source);
                snprintf(seed.source, sizeof(seed.source), "%s", source != NULL ? source : "");
                if (kbo_add_captain_seed_locked(&seed)) {
                    imported++;
                }
            } else if (kbo_captain_seed_line_parse_failure_is_reportable(line)) {
                parse_failed++;
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    if (imported > 0 || parse_failed > 0) {
        append_logf(
            "KBO captain seed import source=%s imported=%d parse_failed=%d path=%s",
            source != NULL ? source : "",
            imported,
            parse_failed,
            path);
    }
    return imported;
}

void kbo_ensure_captain_seeds_loaded(void)
{
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    if (!kbo_get_save_captain_seed_path(save_seed_path, sizeof(save_seed_path))) {
        save_seed_path[0] = '\0';
    }
    if (!kbo_get_global_captain_seed_path(global_seed_path, sizeof(global_seed_path))) {
        global_seed_path[0] = '\0';
    }

    char save_seed_key[MAX_PATH * 2] = {0};
    char global_seed_key[MAX_PATH * 2] = {0};
    kbo_captain_seed_file_loaded_key_component(save_seed_path, save_seed_key, sizeof(save_seed_key));
    kbo_captain_seed_file_loaded_key_component(global_seed_path, global_seed_key, sizeof(global_seed_key));

    char loaded_key[MAX_PATH * 6] = {0};
    snprintf(loaded_key, sizeof(loaded_key), "%s|%s", save_seed_key, global_seed_key);
    if (InterlockedCompareExchange(&g_kbo_captain_seed_loaded, 0, 0) != 0
            && strcmp(g_kbo_captain_seed_loaded_key, loaded_key) == 0) {
        return;
    }

    kbo_lock_captain_seeds();
    if (InterlockedCompareExchange(&g_kbo_captain_seed_loaded, 0, 0) != 0
            && strcmp(g_kbo_captain_seed_loaded_key, loaded_key) == 0) {
        kbo_unlock_captain_seeds();
        return;
    }

    memset(g_kbo_captain_seeds, 0, sizeof(g_kbo_captain_seeds));
    g_kbo_captain_seed_count = 0;
    if (global_seed_path[0] != '\0') {
        kbo_import_captain_seed_file_locked(global_seed_path, "global_seed");
    }
    if (save_seed_path[0] != '\0') {
        kbo_import_captain_seed_file_locked(save_seed_path, "save_seed");
    }
    snprintf(g_kbo_captain_seed_loaded_key, sizeof(g_kbo_captain_seed_loaded_key), "%s", loaded_key);
    InterlockedExchange(&g_kbo_captain_seed_loaded, 1);
    int count = g_kbo_captain_seed_count;
    kbo_unlock_captain_seeds();
    append_logf("KBO captain seeds ready count=%d", count);
}

static int kbo_captain_seed_key_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

static int kbo_captain_seed_key_text_matches(const char* text, const char* key)
{
    char copied[KBO_CAPTAIN_SEED_KEY_BYTES] = {0};
    if (text == NULL || key == NULL || key[0] == '\0') {
        return 0;
    }
    if (!copy_limited_ascii_string(text, copied, sizeof(copied))) {
        return 0;
    }
    return _stricmp(copied, key) == 0;
}

static int kbo_captain_player_string_slot_contains_seed_key(uint8_t* player, uint32_t offset, const char* key)
{
    if (player == NULL || key == NULL || offset >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    char text[KBO_CAPTAIN_SEED_KEY_BYTES] = {0};
    if (offset + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET + sizeof(uintptr_t) <= OOTP27_PLAYER_SCAN_BYTES
            && copy_ootp_string_object_text(player, offset, text, sizeof(text))
            && _stricmp(text, key) == 0) {
        return 1;
    }

    if (offset + sizeof(uintptr_t) <= OOTP27_PLAYER_SCAN_BYTES
            && memory_range_readable(player + offset, sizeof(uintptr_t))) {
        uintptr_t ptr = *(uintptr_t*)(player + offset);
        if (ptr != 0u && kbo_captain_seed_key_text_matches((const char*)ptr, key)) {
            return 1;
        }
    }

    if (kbo_captain_seed_key_text_matches((const char*)(player + offset), key)) {
        return 1;
    }
    return 0;
}

static int kbo_captain_player_contains_seed_key(uint8_t* player, const char* key)
{
    if (player == NULL || key == NULL || key[0] == '\0' || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    size_t key_len = strlen(key);
    if (key_len < 3u || key_len >= KBO_CAPTAIN_SEED_KEY_BYTES || key_len >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    static const uint32_t export_key_offsets[] = { 0x1140u, 0x1188u, 0x11a0u };
    for (int i = 0; i < (int)(sizeof(export_key_offsets) / sizeof(export_key_offsets[0])); i++) {
        if (kbo_captain_player_string_slot_contains_seed_key(player, export_key_offsets[i], key)) {
            return 1;
        }
    }

    for (size_t i = 0; i + key_len < OOTP27_PLAYER_SCAN_BYTES; i++) {
        if (memcmp(player + i, key, key_len) != 0) {
            continue;
        }
        char before = i > 0u ? (char)player[i - 1u] : '\0';
        char after = (char)player[i + key_len];
        if (!kbo_captain_seed_key_char(before) && !kbo_captain_seed_key_char(after)) {
            return 1;
        }
    }
    return 0;
}

static int kbo_captain_seed_matches_team(const KboCaptainSeed* seed, uint32_t team_id, uint8_t* team)
{
    if (seed == NULL) {
        return 0;
    }
    if (seed->team_id != 0u) {
        return seed->team_id == team_id;
    }
    return seed->team_code[0] != '\0'
        && team != NULL
        && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)
        && team_has_ootp_string_text(team, seed->team_code);
}

static int kbo_captain_seed_matches_player(const KboCaptainSeed* seed, uint8_t* player)
{
    if (seed == NULL || player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (seed->player_id != 0u && seed->player_id != player_id) {
        return 0;
    }
    if (seed->player_key[0] != '\0' && !kbo_captain_player_contains_seed_key(player, seed->player_key)) {
        return 0;
    }
    return seed->player_id != 0u || seed->player_key[0] != '\0';
}

static int kbo_captain_seed_match_rank(
    const KboCaptainSeed* seed,
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id)
{
    int rank = (int)seed->source_rank * 1000000;
    if (seed->season == season) {
        rank += 100000;
    }
    if (seed->league_id == league_id) {
        rank += 10000;
    }
    if (seed->team_id == team_id && team_id != 0u) {
        rank += 1000;
    }
    rank += seed->priority;
    return rank;
}

static int kbo_find_best_captain_seed_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* team,
    KboCaptainSeed* out_seed)
{
    if (out_seed != NULL) {
        memset(out_seed, 0, sizeof(*out_seed));
    }
    if (team_id == 0u) {
        return 0;
    }

    int best_rank = -2147483647;
    KboCaptainSeed best;
    memset(&best, 0, sizeof(best));

    kbo_lock_captain_seeds();
    for (int i = 0; i < g_kbo_captain_seed_count; i++) {
        const KboCaptainSeed* seed = &g_kbo_captain_seeds[i];
        if (!seed->active) {
            continue;
        }
        if (seed->season != 0u && seed->season != season) {
            continue;
        }
        if (seed->league_id != 0u && seed->league_id != league_id) {
            continue;
        }
        if (!kbo_captain_seed_matches_team(seed, team_id, team)) {
            continue;
        }

        int rank = kbo_captain_seed_match_rank(seed, season, league_id, team_id);
        if (rank > best_rank) {
            best = *seed;
            best_rank = rank;
        }
    }
    kbo_unlock_captain_seeds();

    if (best_rank == -2147483647) {
        return 0;
    }
    if (out_seed != NULL) {
        *out_seed = best;
    }
    return 1;
}

int kbo_find_captain_seed_for_player(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* team,
    uint8_t* player,
    KboCaptainSeed* out_seed)
{
    if (out_seed != NULL) {
        memset(out_seed, 0, sizeof(*out_seed));
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int best_rank = -2147483647;
    KboCaptainSeed best;
    memset(&best, 0, sizeof(best));

    kbo_lock_captain_seeds();
    for (int i = 0; i < g_kbo_captain_seed_count; i++) {
        const KboCaptainSeed* seed = &g_kbo_captain_seeds[i];
        if (!seed->active) {
            continue;
        }
        if (seed->season != 0u && seed->season != season) {
            continue;
        }
        if (seed->league_id != 0u && seed->league_id != league_id) {
            continue;
        }
        if (!kbo_captain_seed_matches_team(seed, team_id, team)) {
            continue;
        }
        if (seed->player_id != 0u && seed->player_id != player_id) {
            continue;
        }
        if (seed->player_key[0] != '\0' && !kbo_captain_player_contains_seed_key(player, seed->player_key)) {
            continue;
        }

        int rank = kbo_captain_seed_match_rank(seed, season, league_id, team_id);
        if (rank > best_rank) {
            best = *seed;
            best_rank = rank;
        }
    }
    kbo_unlock_captain_seeds();

    if (best_rank == -2147483647) {
        return 0;
    }
    if (out_seed != NULL) {
        *out_seed = best;
    }
    return 1;
}

static int kbo_captain_display_cache_matches(
    const KboCaptainDisplayCache* cache,
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    const char* loaded_key)
{
    return cache != NULL
        && cache->season == season
        && cache->league_id == league_id
        && cache->team_id == team_id
        && strcmp(cache->loaded_key, loaded_key != NULL ? loaded_key : "") == 0;
}

static void kbo_copy_captain_display_outputs(
    const KboCaptainDisplayCache* display,
    char* out_name,
    size_t out_name_size,
    uint32_t* out_player_id,
    char* out_source,
    size_t out_source_size)
{
    if (out_name != NULL && out_name_size > 0u) {
        snprintf(out_name, out_name_size, "%s", display != NULL ? display->player_name : "");
    }
    if (out_player_id != NULL) {
        *out_player_id = display != NULL ? display->player_id : 0u;
    }
    if (out_source != NULL && out_source_size > 0u) {
        snprintf(out_source, out_source_size, "%s", display != NULL ? display->source : "");
    }
}

static void kbo_captain_display_loaded_key(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    char path[MAX_PATH] = {0};
    char csv_key[192] = "csv:missing";
    if (kbo_captain_selection_csv_path(season, path, sizeof(path))) {
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        if (GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
            snprintf(
                csv_key,
                sizeof(csv_key),
                "csv:%08lx%08lx:%08lx%08lx",
                (unsigned long)attrs.ftLastWriteTime.dwHighDateTime,
                (unsigned long)attrs.ftLastWriteTime.dwLowDateTime,
                (unsigned long)attrs.nFileSizeHigh,
                (unsigned long)attrs.nFileSizeLow);
        }
    }

    snprintf(
        out,
        out_size,
        "%s|%s",
        g_kbo_captain_seed_loaded_key,
        csv_key);
}

static uint8_t* kbo_captain_display_find_player_by_id(uint32_t player_id, int* out_vector_available)
{
    if (out_vector_available != NULL) {
        *out_vector_available = 0;
    }
    if (player_id == 0u) {
        return NULL;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }
    if (player_vector == 0u || player_count <= 0 || player_count > 200000
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return NULL;
    }
    if (out_vector_available != NULL) {
        *out_vector_available = 1;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}

static int kbo_captain_display_fill_from_row(
    const KboCaptainSelectionRow* row,
    KboCaptainDisplayCache* out)
{
    if (row == NULL || out == NULL || row->team_id == 0u) {
        return 0;
    }

    int vector_available = 0;
    uint8_t* player = kbo_captain_display_find_player_by_id(row->player_id, &vector_available);
    if (vector_available) {
        if (player == NULL
                || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
                || !kbo_player_current_assignment_matches_team_or_affiliate(player, row->team_id)) {
            return 0;
        }
    }

    out->player_id = row->player_id;
    snprintf(out->player_name, sizeof(out->player_name), "%s", row->player_name);
    snprintf(out->source, sizeof(out->source), "selection_csv");
    if (player != NULL) {
        char resolved_name[128] = {0};
        kbo_copy_player_display_name(player, resolved_name, sizeof(resolved_name));
        if (resolved_name[0] != '\0' && strcmp(resolved_name, "Unknown player") != 0) {
            snprintf(out->player_name, sizeof(out->player_name), "%s", resolved_name);
        }
    }
    out->found = out->player_name[0] != '\0' || out->player_id != 0u;
    return out->found;
}

static int kbo_captain_display_fill_from_selection_csv(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    KboCaptainDisplayCache* out)
{
    if (out == NULL || team_id == 0u) {
        return 0;
    }

    KboCaptainSelectionRow rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(rows, 0, sizeof(rows));
    int row_count = kbo_captain_load_selection_csv(season, rows, KBO_CAPTAIN_MAX_TEAMS);
    for (int i = 0; i < row_count; i++) {
        if (rows[i].team_id != team_id) {
            continue;
        }
        if (rows[i].league_id != 0u && league_id != 0u && rows[i].league_id != league_id) {
            continue;
        }
        return kbo_captain_display_fill_from_row(&rows[i], out);
    }
    return 0;
}

static int kbo_captain_compute_display_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    const char* loaded_key,
    KboCaptainDisplayCache* out)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->season = season;
    out->league_id = league_id;
    out->team_id = team_id;
    snprintf(out->loaded_key, sizeof(out->loaded_key), "%s", loaded_key != NULL ? loaded_key : "");

    if (kbo_captain_display_fill_from_selection_csv(season, league_id, team_id, out)) {
        return 1;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    KboCaptainSeed seed;
    if (!kbo_find_best_captain_seed_for_team(season, league_id, team_id, team, &seed)) {
        return 0;
    }

    snprintf(out->player_name, sizeof(out->player_name), "%s", seed.player_name);
    snprintf(out->source, sizeof(out->source), "%s", seed.source);
    out->player_id = seed.player_id;

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    int vector_available = 0;
    int resolved_seed_player = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            && player_vector != 0u
            && player_count > 0
            && player_count <= 200000
            && memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        vector_available = 1;
        for (int32_t i = 0; i < player_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
                continue;
            }
            if (!kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
                continue;
            }
            if (!kbo_captain_seed_matches_player(&seed, player)) {
                continue;
            }

            out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            resolved_seed_player = 1;
            char resolved_name[128] = {0};
            kbo_copy_player_display_name(player, resolved_name, sizeof(resolved_name));
            if (resolved_name[0] != '\0' && strcmp(resolved_name, "Unknown player") != 0) {
                snprintf(out->player_name, sizeof(out->player_name), "%s", resolved_name);
            }
            break;
        }
    }

    if (vector_available && !resolved_seed_player) {
        out->player_name[0] = '\0';
        out->source[0] = '\0';
        out->player_id = 0u;
        out->found = 0;
        return 0;
    }

    out->found = out->player_name[0] != '\0' || out->player_id != 0u;
    return out->found;
}

int kbo_get_captain_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    char* out_name,
    size_t out_name_size,
    uint32_t* out_player_id,
    char* out_source,
    size_t out_source_size)
{
    if (out_name != NULL && out_name_size > 0u) {
        out_name[0] = '\0';
    }
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (out_source != NULL && out_source_size > 0u) {
        out_source[0] = '\0';
    }
    if (season == 0u || team_id == 0u) {
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();

    char loaded_key[MAX_PATH * 6] = {0};
    kbo_captain_display_loaded_key(season, loaded_key, sizeof(loaded_key));

    kbo_lock_captain_display_cache();
    if (kbo_captain_display_cache_matches(
            &g_kbo_captain_display_cache,
            season,
            league_id,
            team_id,
            loaded_key)
            && (out_player_id == NULL
                || g_kbo_captain_display_cache.player_id != 0u
                || !g_kbo_captain_display_cache.found)) {
        KboCaptainDisplayCache cached = g_kbo_captain_display_cache;
        kbo_unlock_captain_display_cache();
        kbo_copy_captain_display_outputs(
            &cached,
            out_name,
            out_name_size,
            out_player_id,
            out_source,
            out_source_size);
        return cached.found;
    }
    kbo_unlock_captain_display_cache();

    KboCaptainDisplayCache computed;
    kbo_captain_compute_display_for_team(season, league_id, team_id, loaded_key, &computed);

    kbo_lock_captain_display_cache();
    g_kbo_captain_display_cache = computed;
    kbo_unlock_captain_display_cache();

    kbo_copy_captain_display_outputs(
        &computed,
        out_name,
        out_name_size,
        out_player_id,
        out_source,
        out_source_size);
    return computed.found;
}

int kbo_captain_player_is_team_captain(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    uint32_t captain_player_id = 0u;
    if (!kbo_get_captain_for_team(
            season,
            league_id,
            team_id,
            NULL,
            0u,
            &captain_player_id,
            NULL,
            0u)) {
        return 0;
    }
    return captain_player_id != 0u && captain_player_id == player_id;
}
