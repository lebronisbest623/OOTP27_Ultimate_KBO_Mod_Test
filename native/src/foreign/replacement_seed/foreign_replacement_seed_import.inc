/* Foreign replacement-player seed import table. Included from foreign_replacement_seed.inc. */

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

