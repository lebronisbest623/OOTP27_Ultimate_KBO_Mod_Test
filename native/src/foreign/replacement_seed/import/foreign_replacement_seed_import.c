#include "../internal/foreign_replacement_seed_internal.h"

void kbo_lock_foreign_replacement_player_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_replacement_player_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_unlock_foreign_replacement_player_seeds(void)
{
    InterlockedExchange(&g_kbo_foreign_replacement_player_seed_lock, 0);
}

int kbo_player_memory_contains_ascii_seed_key(uint8_t* player, const char* key)
{
    if (player == NULL || key == NULL || key[0] == '\0' || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    size_t key_len = strlen(key);
    if (key_len < 3u || key_len >= KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES || key_len >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    for (size_t i = 0; i + key_len < OOTP27_PLAYER_SCAN_BYTES; i++) {
        if (memcmp(player + i, key, key_len) != 0) {
            continue;
        }
        char before = i > 0u ? (char)player[i - 1u] : '\0';
        char after = (char)player[i + key_len];
        if (!kbo_ascii_is_seed_id_char(before) && !kbo_ascii_is_seed_id_char(after)) {
            return 1;
        }
    }
    return 0;
}

uint32_t kbo_resolve_foreign_replacement_player_seed_key(const char* key, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    if (key == NULL || key[0] == '\0') {
        return 0u;
    }
    if (key[0] >= '0' && key[0] <= '9') {
        return (uint32_t)strtoul(key, NULL, 10);
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0u;
    }

    uint32_t first_match = 0u;
    uint8_t first_slot_type = 0u;
    int matches = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_player_memory_contains_ascii_seed_key(player, key)) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        if (first_match == 0u) {
            first_match = player_id;
            first_slot_type = kbo_player_is_asian_quota_candidate(player)
                ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                : KBO_FOREIGN_INJURY_SLOT_REGULAR;
        }
        matches++;
    }

    if (first_match != 0u) {
        if (out_slot_type != NULL) {
            *out_slot_type = first_slot_type;
        }
        append_logf(
            "foreign replacement player seed resolved key=%s player=%u matches=%d slot=%s",
            key,
            first_match,
            matches,
            first_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular");
    }
    return first_match;
}

int kbo_buffer_contains_u32_le(const uint8_t* data, size_t size, uint32_t value)
{
    if (data == NULL || size < 4u || value == 0u) {
        return 0;
    }
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
    bytes[2] = (uint8_t)((value >> 16) & 0xffu);
    bytes[3] = (uint8_t)((value >> 24) & 0xffu);
    for (size_t i = 0; i + 4u <= size; i++) {
        if (memcmp(data + i, bytes, sizeof(bytes)) == 0) {
            return 1;
        }
    }
    return 0;
}

uint32_t kbo_foreign_resolve_player_id_from_players_dat_record_start(
    const uint8_t* raw,
    size_t read,
    size_t key_pos)
{
    if (raw == NULL || read < 16u || key_pos < 16u) {
        return 0u;
    }

    size_t min_start = key_pos > 512u ? key_pos - 512u : 0u;
    for (size_t start = key_pos - 16u; ; start--) {
        if (start + 16u <= read) {
            uint32_t candidate_id =
                (uint32_t)raw[start]
                | ((uint32_t)raw[start + 1u] << 8)
                | ((uint32_t)raw[start + 2u] << 16)
                | ((uint32_t)raw[start + 3u] << 24);
            uint32_t first_name_id =
                (uint32_t)raw[start + 4u]
                | ((uint32_t)raw[start + 5u] << 8)
                | ((uint32_t)raw[start + 6u] << 16)
                | ((uint32_t)raw[start + 7u] << 24);
            uint32_t last_name_id =
                (uint32_t)raw[start + 8u]
                | ((uint32_t)raw[start + 9u] << 8)
                | ((uint32_t)raw[start + 10u] << 16)
                | ((uint32_t)raw[start + 11u] << 24);
            uint8_t day = raw[start + 12u];
            uint8_t month = raw[start + 13u];
            uint16_t year = (uint16_t)raw[start + 14u] | ((uint16_t)raw[start + 15u] << 8);
            if (candidate_id != 0u
                    && first_name_id != 0u
                    && last_name_id != 0u
                    && day >= 1u && day <= 31u
                    && month >= 1u && month <= 12u
                    && year >= 1800u && year <= 2100u) {
                uint8_t* candidate = kbo_find_player_by_id(candidate_id, NULL, NULL);
                if (candidate != NULL && kbo_player_pointer_plausible((uintptr_t)candidate)) {
                    return candidate_id;
                }
            }
        }
        if (start == min_start || start == 0u) {
            break;
        }
    }
    return 0u;
}

uint32_t kbo_resolve_foreign_replacement_player_seed_from_players_dat(const char* key, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    if (key == NULL || key[0] == '\0' || (key[0] >= '0' && key[0] <= '9')) {
        return 0u;
    }

    char players_dat_path[MAX_PATH] = {0};
    if (!kbo_get_current_players_dat_path(players_dat_path, sizeof(players_dat_path))) {
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat missing or save not written", key);
        return 0u;
    }

    HANDLE file = CreateFileA(players_dat_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat open failed gle=%lu", key, (unsigned long)GetLastError());
        return 0u;
    }

    LARGE_INTEGER file_size;
    file_size.QuadPart = 0;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 512ll * 1024ll * 1024ll) {
        CloseHandle(file);
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat size unsupported", key);
        return 0u;
    }

    uint8_t* raw = (uint8_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)file_size.QuadPart);
    if (raw == NULL) {
        CloseHandle(file);
        return 0u;
    }

    DWORD read = 0;
    int read_ok = ReadFile(file, raw, (DWORD)file_size.QuadPart, &read, NULL) && read == (DWORD)file_size.QuadPart;
    CloseHandle(file);
    if (!read_ok) {
        HeapFree(GetProcessHeap(), 0, raw);
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat read failed", key);
        return 0u;
    }

    size_t key_len = strlen(key);
    uint32_t matched_player_id = 0u;
    uint8_t matched_slot_type = 0u;
    int ambiguous = 0;
    for (size_t pos = 0; pos + key_len <= (size_t)read; pos++) {
        if (memcmp(raw + pos, key, key_len) != 0) {
            continue;
        }
        char before = pos > 0u ? (char)raw[pos - 1u] : '\0';
        char after = (char)raw[pos + key_len];
        if (kbo_ascii_is_seed_id_char(before) || kbo_ascii_is_seed_id_char(after)) {
            continue;
        }

        uint32_t record_player_id = kbo_foreign_resolve_player_id_from_players_dat_record_start(raw, (size_t)read, pos);
        if (record_player_id != 0u) {
            uint8_t* candidate = kbo_find_player_by_id(record_player_id, NULL, NULL);
            if (matched_player_id != 0u && matched_player_id != record_player_id) {
                ambiguous = 1;
                break;
            }
            matched_player_id = record_player_id;
            matched_slot_type = candidate != NULL && kbo_player_is_asian_quota_candidate(candidate)
                ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                : KBO_FOREIGN_INJURY_SLOT_REGULAR;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    if (ambiguous) {
        append_logf("foreign replacement player seed unresolved key=%s reason=players.dat ambiguous", key);
        return 0u;
    }
    if (matched_player_id != 0u) {
        if (out_slot_type != NULL) {
            *out_slot_type = matched_slot_type;
        }
        append_logf(
            "foreign replacement player seed resolved via players.dat key=%s player=%u slot=%s",
            key,
            matched_player_id,
            matched_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular");
    }
    return matched_player_id;
}

int kbo_add_foreign_replacement_player_seed_locked(const KboForeignReplacementPlayerSeed* seed)
{
    if (seed == NULL || (seed->key[0] == '\0' && seed->player_id == 0u)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* existing = &g_kbo_foreign_replacement_player_seeds[i];
        if ((seed->player_id != 0u && existing->player_id == seed->player_id)
                || (seed->key[0] != '\0' && existing->key[0] != '\0' && _stricmp(existing->key, seed->key) == 0)) {
            if (existing->slot_type == 0u && seed->slot_type != 0u) {
                existing->slot_type = seed->slot_type;
            }
            return 0;
        }
    }
    if (g_kbo_foreign_replacement_player_seed_count >= KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX) {
        return 0;
    }
    g_kbo_foreign_replacement_player_seeds[g_kbo_foreign_replacement_player_seed_count++] = *seed;
    return 1;
}

int kbo_import_foreign_replacement_player_seed_file_locked(const char* path, const char* source)
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
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[180] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            KboForeignReplacementPlayerSeed seed;
            if (kbo_parse_foreign_replacement_player_seed_line(line, &seed)
                    && kbo_add_foreign_replacement_player_seed_locked(&seed)) {
                imported++;
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    if (imported > 0) {
        append_logf(
            "foreign replacement player seed import source=%s imported=%d path=%s",
            source != NULL ? source : "",
            imported,
            path);
    }
    return imported;
}

