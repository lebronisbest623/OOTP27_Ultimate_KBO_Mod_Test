#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_atomic_file.h"
#include "../../core/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/team_lookup.h"
#include "../foreign_waiver_player_eval.h"
#include "foreign_replacement_seed_paths.h"
#include "foreign_replacement_seed_parse.h"
/* Foreign replacement-player seed resolution. Included from native/src/foreign_waiver_ai.inc. */

#include "foreign_replacement_seed_parse.h"

#define KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX 128

static KboForeignReplacementPlayerSeed g_kbo_foreign_replacement_player_seeds[KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX];
static int g_kbo_foreign_replacement_player_seed_count = 0;
static char g_kbo_foreign_replacement_player_seed_loaded_path[MAX_PATH] = {0};
static LONG g_kbo_foreign_replacement_player_seed_lock = 0;
static LONG64 g_kbo_foreign_replacement_player_seed_last_resolve_tick = 0;

static int kbo_persist_foreign_replacement_player_resolved_cache_locked(void);


/* Foreign replacement-player seed lock helpers. Included from native/KBOFix.c. */

static void kbo_lock_foreign_replacement_player_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_replacement_player_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_foreign_replacement_player_seeds(void)
{
    InterlockedExchange(&g_kbo_foreign_replacement_player_seed_lock, 0);
}

/* Foreign replacement-player seed memory-key resolution. Included from native/KBOFix.c. */

static int kbo_player_memory_contains_ascii_seed_key(uint8_t* player, const char* key)
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

static uint32_t kbo_resolve_foreign_replacement_player_seed_key(const char* key, uint8_t* out_slot_type)
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

/* Foreign replacement-player seed players.dat resolution. Included from native/KBOFix.c. */

static int kbo_buffer_contains_u32_le(const uint8_t* data, size_t size, uint32_t value)
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

static uint32_t kbo_foreign_resolve_player_id_from_players_dat_record_start(
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

static uint32_t kbo_resolve_foreign_replacement_player_seed_from_players_dat(const char* key, uint8_t* out_slot_type)
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


/* Foreign replacement-player seed import table. Included from native/KBOFix.c. */

static int kbo_add_foreign_replacement_player_seed_locked(const KboForeignReplacementPlayerSeed* seed)
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

static int kbo_import_foreign_replacement_player_seed_file_locked(const char* path, const char* source)
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

/* Foreign replacement-player resolved cache IO. Included from native/KBOFix.c. */

static int kbo_load_foreign_replacement_player_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_foreign_replacement_players_resolved_path(path, sizeof(path))) {
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
    int loaded = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[160] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            char* key = line;
            char* comma = strchr(key, ',');
            if (comma != NULL) {
                *comma = '\0';
                char* player_text = comma + 1;
                char* second_comma = strchr(player_text, ',');
                char* slot_text = NULL;
                if (second_comma != NULL) {
                    *second_comma = '\0';
                    slot_text = second_comma + 1;
                    char* third_comma = strchr(slot_text, ',');
                    if (third_comma != NULL) {
                        *third_comma = '\0';
                    }
                }
                kbo_trim_csv_token_in_place(key);
                kbo_trim_csv_token_in_place(player_text);
                if (slot_text != NULL) {
                    kbo_trim_csv_token_in_place(slot_text);
                }
                if (key[0] != '\0' && _stricmp(key, "source_key") != 0 && player_text[0] >= '0' && player_text[0] <= '9') {
                    uint32_t player_id = (uint32_t)strtoul(player_text, NULL, 10);
                    uint8_t slot_type = kbo_parse_foreign_replacement_seed_slot_type(slot_text);
                    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
                        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
                        if (seed->key[0] != '\0' && _stricmp(seed->key, key) == 0) {
                            seed->player_id = player_id;
                            if (seed->slot_type == 0u) {
                                seed->slot_type = slot_type;
                            }
                            loaded++;
                            break;
                        }
                    }
                }
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    if (loaded > 0) {
        append_logf("foreign replacement player resolved cache loaded=%d path=%s", loaded, path);
    }
    return loaded;
}

static int kbo_persist_foreign_replacement_player_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_foreign_replacement_players_resolved_path(path, sizeof(path))) {
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign replacement player resolved cache persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }

    DWORD wrote = 0;
    const char* header = "source_key,player_id,slot_type\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &wrote, NULL);
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
        if (seed->key[0] == '\0' || seed->player_id == 0u) {
            continue;
        }
        char line[160] = {0};
        int len = snprintf(line, sizeof(line), "%s,%u,%u\r\n", seed->key, seed->player_id, (uint32_t)seed->slot_type);
        if (len > 0 && len < (int)sizeof(line)) {
            WriteFile(file, line, (DWORD)len, &wrote, NULL);
        }
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        append_logf("foreign replacement player resolved cache: atomic commit failed path=%s", path);
        return 0;
    }
    return 1;
}

/* Foreign replacement-player seed lazy-load and resolve orchestration. Included from native/KBOFix.c. */

static int kbo_resolve_foreign_replacement_player_seeds_locked(void)
{
    int resolved = 0;
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
        if (seed->player_id != 0u) {
            continue;
        }
        uint8_t resolved_slot_type = 0u;
        uint32_t player_id = 0u;
        if (seed->key[0] >= '0' && seed->key[0] <= '9') {
            player_id = (uint32_t)strtoul(seed->key, NULL, 10);
        } else {
            player_id = kbo_resolve_foreign_replacement_player_seed_from_players_dat(seed->key, &resolved_slot_type);
        }
        if (player_id != 0u) {
            seed->player_id = player_id;
            if (seed->slot_type == 0u) {
                seed->slot_type = resolved_slot_type;
            }
            resolved++;
        }
    }
    InterlockedExchange64(&g_kbo_foreign_replacement_player_seed_last_resolve_tick, (LONG64)GetTickCount64());
    if (resolved > 0) {
        kbo_persist_foreign_replacement_player_resolved_cache_locked();
    }
    return resolved;
}

void kbo_ensure_foreign_replacement_player_seeds_loaded(void)
{
    char current_path[MAX_PATH] = {0};
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    if (!kbo_get_save_foreign_replacement_players_seed_path(save_seed_path, sizeof(save_seed_path))) {
        save_seed_path[0] = '\0';
    }
    if (!kbo_get_global_foreign_replacement_players_seed_path(global_seed_path, sizeof(global_seed_path))) {
        global_seed_path[0] = '\0';
    }
    snprintf(current_path, sizeof(current_path), "%s", save_seed_path[0] != '\0' ? save_seed_path : global_seed_path);

    kbo_lock_foreign_replacement_player_seeds();
    if (strcmp(g_kbo_foreign_replacement_player_seed_loaded_path, current_path) != 0) {
        memset(g_kbo_foreign_replacement_player_seeds, 0, sizeof(g_kbo_foreign_replacement_player_seeds));
        g_kbo_foreign_replacement_player_seed_count = 0;
        snprintf(g_kbo_foreign_replacement_player_seed_loaded_path, sizeof(g_kbo_foreign_replacement_player_seed_loaded_path), "%s", current_path);
        if (save_seed_path[0] != '\0') {
            kbo_import_foreign_replacement_player_seed_file_locked(save_seed_path, "save_seed");
        }
        if (global_seed_path[0] != '\0') {
            kbo_import_foreign_replacement_player_seed_file_locked(global_seed_path, "global_seed");
        }
        kbo_load_foreign_replacement_player_resolved_cache_locked();
        kbo_resolve_foreign_replacement_player_seeds_locked();
    } else {
        int unresolved = 0;
        for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
            if (g_kbo_foreign_replacement_player_seeds[i].player_id == 0u) {
                unresolved++;
            }
        }
        LONG64 last_tick = InterlockedCompareExchange64(&g_kbo_foreign_replacement_player_seed_last_resolve_tick, 0, 0);
        ULONGLONG now = GetTickCount64();
        if (unresolved > 0 && (last_tick <= 0 || now < (ULONGLONG)last_tick || now - (ULONGLONG)last_tick > 5000ull)) {
            kbo_resolve_foreign_replacement_player_seeds_locked();
        }
    }
    kbo_unlock_foreign_replacement_player_seeds();
}

/* Foreign replacement-player seed match API. Included from native/KBOFix.c. */

int kbo_foreign_replacement_player_seed_matches_loaded(uint8_t* player, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return 0;
    }

    int matched = 0;
    kbo_lock_foreign_replacement_player_seeds();
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
        if (seed->player_id == player_id) {
            if (out_slot_type != NULL) {
                *out_slot_type = seed->slot_type;
            }
            matched = 1;
            break;
        }
    }
    kbo_unlock_foreign_replacement_player_seeds();
    return matched;
}

