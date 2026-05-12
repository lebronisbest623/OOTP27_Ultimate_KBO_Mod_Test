#include "../internal/foreign_replacement_seed_internal.h"

int kbo_load_foreign_replacement_player_resolved_cache_locked(void)
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

int kbo_persist_foreign_replacement_player_resolved_cache_locked(void)
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

int kbo_resolve_foreign_replacement_player_seeds_locked(void)
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
            player_id = kbo_resolve_foreign_replacement_player_seed_key(seed->key, &resolved_slot_type);
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

