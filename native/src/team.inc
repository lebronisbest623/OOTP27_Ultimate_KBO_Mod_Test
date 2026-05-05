/* String helpers (from league_team_rules) */

static int copy_limited_ascii_string(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (source == NULL || !memory_range_readable(source, 1)) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < 512; len++) {
        if (!memory_range_readable(source + len, 1)) {
            break;
        }
        unsigned char c = (unsigned char)source[len];
        if (c == '\0') {
            break;
        }
        if (c < 0x20u || c > 0x7eu) {
            break;
        }
        out[len] = (char)c;
    }

    out[len] = '\0';
    return len > 0;
}

static int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size)
{
    uint8_t* string_object = object_base + string_offset;
    uint8_t* text_slot     = string_object + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET;
    if (!memory_range_readable(text_slot, sizeof(char*))) {
        return 0;
    }

    const char* text = *(const char**)text_slot;
    return copy_limited_ascii_string(text, out, out_size);
}

static int team_has_ootp_string_text(uint8_t* team, const char* expected)
{
    if (team == NULL || expected == NULL || expected[0] == '\0') {
        return 0;
    }

    static const uint32_t string_offsets[] = {
        0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u
    };

    char text[96] = {0};
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        text[0] = '\0';
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && ascii_equals_ignore_case(text, expected)) {
            return 1;
        }
    }

    return 0;
}

#define KBO_NAME_ID_CACHE_MAX       300000u
#define KBO_NAME_CACHE_TEXT_BYTES   48u
#define KBO_NAMES_DAT_MAX_BYTES     (32u * 1024u * 1024u)

static char* g_kbo_name_cache_texts = NULL;
static char  g_kbo_name_cache_save_path[MAX_PATH] = {0};
static LONG  g_kbo_name_cache_loading = 0;

static uint32_t kbo_read_u32_le_bytes(const uint8_t* data)
{
    return (uint32_t)data[0]
        | ((uint32_t)data[1] << 8)
        | ((uint32_t)data[2] << 16)
        | ((uint32_t)data[3] << 24);
}

static int kbo_name_record_text_plausible(const uint8_t* text, uint32_t len)
{
    if (text == NULL || len == 0 || len >= KBO_NAME_CACHE_TEXT_BYTES) {
        return 0;
    }

    int has_letter = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t c = text[i];
        if (c == 0 || c < 0x20u) {
            return 0;
        }
        if (c >= 0x80u) {
            has_letter = 1;
            continue;
        }
        if (c > 0x7eu || c == '<' || c == '>' || c == '\\' || c == '/') {
            return 0;
        }
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            has_letter = 1;
        }
    }

    return has_letter;
}

static void kbo_name_cache_store(char* cache, uint32_t id, const uint8_t* text, uint32_t len)
{
    if (cache == NULL || id == 0 || id > KBO_NAME_ID_CACHE_MAX
            || !kbo_name_record_text_plausible(text, len)) {
        return;
    }

    char* slot = cache + ((SIZE_T)id * KBO_NAME_CACHE_TEXT_BYTES);
    if (slot[0] != '\0') {
        return;
    }

    SIZE_T copy_len = len;
    if (copy_len >= KBO_NAME_CACHE_TEXT_BYTES) {
        copy_len = KBO_NAME_CACHE_TEXT_BYTES - 1u;
    }
    memcpy(slot, text, copy_len);
    slot[copy_len] = '\0';
}

static int kbo_load_name_cache_for_save(const char* save_path)
{
    if (save_path == NULL || save_path[0] == '\0') {
        return 0;
    }

    if (g_kbo_name_cache_texts != NULL
            && strcmp(g_kbo_name_cache_save_path, save_path) == 0) {
        return 1;
    }

    if (InterlockedCompareExchange(&g_kbo_name_cache_loading, 1, 0) != 0) {
        return g_kbo_name_cache_texts != NULL
            && strcmp(g_kbo_name_cache_save_path, save_path) == 0;
    }

    if (g_kbo_name_cache_texts != NULL) {
        HeapFree(GetProcessHeap(), 0, g_kbo_name_cache_texts);
        g_kbo_name_cache_texts = NULL;
        g_kbo_name_cache_save_path[0] = '\0';
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\names.dat", save_path);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO names.dat cache skipped reason=open_failed gle=%lu path=%s", GetLastError(), path);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0
            || file_size.QuadPart > (LONGLONG)KBO_NAMES_DAT_MAX_BYTES) {
        append_logf("KBO names.dat cache skipped reason=bad_size size=%lld path=%s", file_size.QuadPart, path);
        CloseHandle(file);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    DWORD bytes_to_read = (DWORD)file_size.QuadPart;
    uint8_t* data = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, bytes_to_read);
    SIZE_T table_bytes = ((SIZE_T)KBO_NAME_ID_CACHE_MAX + 1u) * KBO_NAME_CACHE_TEXT_BYTES;
    char* cache = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, table_bytes);
    if (data == NULL || cache == NULL) {
        append_logf("KBO names.dat cache skipped reason=alloc_failed bytes=%lu table=%llu", bytes_to_read, (unsigned long long)table_bytes);
        if (data != NULL) { HeapFree(GetProcessHeap(), 0, data); }
        if (cache != NULL) { HeapFree(GetProcessHeap(), 0, cache); }
        CloseHandle(file);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    DWORD read = 0;
    BOOL ok = ReadFile(file, data, bytes_to_read, &read, NULL);
    CloseHandle(file);
    if (!ok || read != bytes_to_read) {
        append_logf("KBO names.dat cache skipped reason=read_failed gle=%lu read=%lu expected=%lu path=%s",
            GetLastError(), read, bytes_to_read, path);
        HeapFree(GetProcessHeap(), 0, data);
        HeapFree(GetProcessHeap(), 0, cache);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    uint32_t loaded = 0;
    for (DWORD i = 0; i + 13u < read; ) {
        uint8_t tag = data[i];
        if (tag != 0x27u && tag != 0x07u) {
            i++;
            continue;
        }

        uint32_t len = kbo_read_u32_le_bytes(data + i + 1u);
        if (len == 0 || len >= KBO_NAME_CACHE_TEXT_BYTES
                || i + 5u + len + 8u > read) {
            i++;
            continue;
        }

        uint32_t text_offset = i + 5u;
        uint32_t id_offset   = text_offset + len + 4u;
        if (data[text_offset + len] != 0u
                || data[text_offset + len + 1u] != 0u
                || data[text_offset + len + 2u] != 0u
                || data[text_offset + len + 3u] != 0u) {
            i++;
            continue;
        }

        uint32_t id = kbo_read_u32_le_bytes(data + id_offset);
        if (id > 0 && id <= KBO_NAME_ID_CACHE_MAX
                && kbo_name_record_text_plausible(data + text_offset, len)) {
            char* slot = cache + ((SIZE_T)id * KBO_NAME_CACHE_TEXT_BYTES);
            if (slot[0] == '\0') {
                kbo_name_cache_store(cache, id, data + text_offset, len);
                loaded++;
            }
            i = id_offset + sizeof(uint32_t);
            continue;
        }

        i++;
    }

    HeapFree(GetProcessHeap(), 0, data);
    if (loaded == 0) {
        HeapFree(GetProcessHeap(), 0, cache);
        append_logf("KBO names.dat cache skipped reason=no_records path=%s", path);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    g_kbo_name_cache_texts = cache;
    snprintf(g_kbo_name_cache_save_path, sizeof(g_kbo_name_cache_save_path), "%s", save_path);
    append_logf("KBO names.dat cache loaded entries=%u path=%s", loaded, path);

    InterlockedExchange(&g_kbo_name_cache_loading, 0);
    return 1;
}

static int kbo_lookup_name_id(uint32_t name_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (name_id == 0 || name_id > KBO_NAME_ID_CACHE_MAX) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))
            || !kbo_load_name_cache_for_save(save_path)) {
        return 0;
    }

    char* slot = g_kbo_name_cache_texts + ((SIZE_T)name_id * KBO_NAME_CACHE_TEXT_BYTES);
    if (slot[0] == '\0') {
        return 0;
    }

    snprintf(out, out_size, "%s", slot);
    return 1;
}

static void kbo_copy_player_display_name(uint8_t* player, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint32_t player_id = 0;
    if (player != NULL && memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    }

    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        snprintf(out, out_size, player_id != 0 ? "Player #%u" : "Unknown player", player_id);
        return;
    }

    uint32_t given_name_id  = *(uint32_t*)(player + OOTP27_PLAYER_GIVEN_NAME_ID_OFFSET);
    uint32_t family_name_id = *(uint32_t*)(player + OOTP27_PLAYER_FAMILY_NAME_ID_OFFSET);

    char given[64]  = {0};
    char family[64] = {0};
    int has_given  = kbo_lookup_name_id(given_name_id, given, sizeof(given));
    int has_family = kbo_lookup_name_id(family_name_id, family, sizeof(family));

    if (has_family && has_given) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(out, out_size, "%s %s", family, given);
#pragma GCC diagnostic pop
    } else if (has_given) {
        snprintf(out, out_size, "%s", given);
    } else if (has_family) {
        snprintf(out, out_size, "%s", family);
    } else if (player_id != 0) {
        snprintf(out, out_size, "Player #%u", player_id);
    } else {
        snprintf(out, out_size, "Unknown player");
    }
}

/* Team and player lookup (from team_continuity) */

static int kbo_player_pointer_plausible(uintptr_t player_ptr)
{
    if (player_ptr == 0 || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player   = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age       = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    return player_id > 0 && player_id < 200000000u && age < 80;
}

static int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset)
{
    if (out_vector != NULL) { *out_vector = 0; }
    if (out_count  != NULL) { *out_count  = 0; }
    if (out_offset != NULL) { *out_offset = 0; }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    int best_matches     = 0;
    uintptr_t best_vector = 0;
    int32_t   best_count  = 0;
    uint32_t  best_offset = 0;

    for (uint32_t offset = 0x20u; offset <= 0x500u; offset += 8u) {
        if (!memory_range_readable((void*)(global + offset), 0x10)) {
            continue;
        }

        uintptr_t vector = *(uintptr_t*)(global + offset);
        int32_t   count  = *(int32_t*)(global + offset + 0x0cu);
        if (vector == 0 || count <= 0 || count > 200000) {
            continue;
        }
        if (!memory_range_readable((void*)vector, (SIZE_T)count * sizeof(uintptr_t))) {
            continue;
        }

        int matches = 0;
        int sample_count = count < 2000 ? count : 2000;
        for (int32_t i = 0; i < sample_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (kbo_player_pointer_plausible(player_ptr)) {
                matches++;
            }
        }

        if (matches > best_matches) {
            best_matches = matches;
            best_vector  = vector;
            best_count   = count;
            best_offset  = offset;
        }
    }

    if (best_matches <= 0 || best_vector == 0 || best_count <= 0) {
        return 0;
    }

    if (out_vector != NULL) { *out_vector = best_vector; }
    if (out_count  != NULL) { *out_count  = best_count;  }
    if (out_offset != NULL) { *out_offset = best_offset; }
    return 1;
}

static uint8_t* find_kbo_team_by_csv_id_any_league(const char* team_id, int allow_deleted)
{
    if (team_id == NULL || team_id[0] == '\0') {
        return NULL;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t   team_count  = *(int32_t*)(global  + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return NULL;
    }

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (!allow_deleted && team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (team_has_ootp_string_text(team, team_id)) {
            return team;
        }
    }

    return NULL;
}

static uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted)
{
    if (team_id == 0) {
        return NULL;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t   team_count  = *(int32_t*)(global  + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return NULL;
    }

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (!allow_deleted && team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) == team_id) {
            return team;
        }
    }

    return NULL;
}

/* Roster array helpers */

static int kbo_remove_player_id_from_team_fixed_array(uint8_t* team, uint32_t array_offset, uint32_t player_id)
{
    if (team == NULL || player_id == 0 || !memory_range_readable(team + array_offset, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    int removed = 0;
    uint32_t* ids = (uint32_t*)(team + array_offset);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == player_id) {
            ids[i] = 0;
            removed++;
        }
    }
    return removed;
}

static int kbo_add_player_id_to_team_fixed_array(uint8_t* team, uint32_t array_offset, uint32_t player_id)
{
    if (team == NULL || player_id == 0 || !memory_range_readable(team + array_offset, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    uint32_t* ids = (uint32_t*)(team + array_offset);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == player_id) {
            return 1;
        }
    }
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == 0) {
            ids[i] = player_id;
            return 1;
        }
    }
    return 0;
}

static int kbo_remove_player_id_from_known_team_roster_arrays(uint8_t* team, uint32_t player_id)
{
    if (team == NULL || player_id == 0) {
        return 0;
    }

    int removed = 0;
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2DA0_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_33E0_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_3700_OFFSET, player_id);
    return removed;
}

static int kbo_add_player_id_to_team_assignment_arrays(uint8_t* team, uint32_t player_id)
{
    int added = 0;
    added += kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id);
    added += kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id);
    return added;
}

/* Team assignment (from team_continuity) */

typedef uint8_t (__fastcall *OotpTeamAddPlayerFn)(void* team, void* player);
typedef void    (__fastcall *OotpTeamRemovePlayerIdFn)(void* team, uint32_t player_id, uint8_t r2760, uint8_t r2da0, uint8_t r3700);

static OotpTeamAddPlayerFn get_ootp_team_add_player_fn(void)
{
    static int logged = 0;
    if (!logged) {
        logged = 1;
        append_log_line("team add player RVA removed; using fallback array update path.");
    }
    return NULL;
}

static OotpTeamRemovePlayerIdFn get_ootp_team_remove_player_id_fn(void)
{
    static int logged = 0;
    if (!logged) {
        logged = 1;
        append_log_line("team remove player RVA removed; using fallback array update path.");
    }
    return NULL;
}

static void kbo_assign_player_to_team_internal(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int update_organization,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach)
{
    if (out_called_pre_change != NULL) { *out_called_pre_change = 0; }
    if (out_called_register   != NULL) { *out_called_register   = 0; }
    if (out_called_attach     != NULL) { *out_called_attach     = 0; }

    if (player == NULL || team == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(team, OOTP27_KBO_TEAM_ID_OFFSET + sizeof(uint32_t))) {
        return;
    }

    uint32_t to_team_numeric_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (to_team_numeric_id == 0) {
        return;
    }

    uint32_t player_id                  = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id            = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t current_league_id          = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t original_organization_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == 0) {
        team_league_id = fallback_league_id;
    }

    uint32_t parent_team_id = 0;
    if (memory_range_readable(team + 0x130, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + 0x130);
    }

    uint8_t* old_team = NULL;
    if (current_team_id != 0 && current_team_id != to_team_numeric_id) {
        old_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
    }

    OotpTeamRemovePlayerIdFn remove_player_id = get_ootp_team_remove_player_id_fn();
    if (old_team != NULL && player_id != 0) {
        if (remove_player_id != NULL) {
            remove_player_id(old_team, player_id, 1, 1, 1);
            if (out_called_pre_change != NULL) { *out_called_pre_change = 1; }
        } else if (kbo_remove_player_id_from_known_team_roster_arrays(old_team, player_id) > 0
                && out_called_pre_change != NULL) {
            *out_called_pre_change = 1;
        }
    }

    uint8_t added_by_ootp = 0;
    OotpTeamAddPlayerFn add_player = get_ootp_team_add_player_fn();
    if (add_player != NULL) {
        added_by_ootp = add_player(team, player);
        if (out_called_register != NULL) { *out_called_register = 1; }
        if (added_by_ootp && out_called_attach != NULL) { *out_called_attach = 1; }
    }

    if (!added_by_ootp) {
        if (player_id != 0) {
            kbo_add_player_id_to_team_assignment_arrays(team, player_id);
        }
        if (current_team_id != 0 && *(uint32_t*)(player + 0x5c) != current_team_id) {
            *(uint32_t*)(player + 0x5c) = current_team_id;
        }
        if (current_league_id != 0 && *(uint32_t*)(player + 0x4c) != current_league_id) {
            *(uint32_t*)(player + 0x4c) = current_league_id;
        }
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)   = to_team_numeric_id;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = team_league_id;
        *(uint16_t*)(player + 0x41) = 0x0100;
    }

    if (update_organization) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) =
            parent_team_id != 0 ? parent_team_id : to_team_numeric_id;
    } else {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_organization_team;
    }
    *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = fallback_league_id;
}

static void kbo_assign_player_to_team_like_ootp(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach)
{
    kbo_assign_player_to_team_internal(
        player, team, fallback_league_id, 1,
        out_called_pre_change, out_called_register, out_called_attach);
}
