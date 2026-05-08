/* Foreign injury replacement CSV loading. Included from native/KBOFix.c. */

static int kbo_load_foreign_injury_replacements_locked(const char* path)
{
    g_kbo_foreign_injury_replacement_count = 0;
    if (path == NULL || path[0] == '\0') {
        g_kbo_foreign_injury_replacement_loaded_path[0] = '\0';
        return 0;
    }
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 262144u) {
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
        while (*cursor != '\0' && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[256] = {0};
            size_t len = (size_t)(next - cursor);
            while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                len--;
            }
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);

            const char* p = line;
            uint32_t team_id = 0u;
            uint32_t league_id = 0u;
            uint32_t injured_player_id = 0u;
            uint32_t replacement_player_id = 0u;
            uint32_t opened_on = 0u;
            uint32_t expected_end = 0u;
            uint32_t slot_type = 0u;
            uint32_t status = 0u;
            uint32_t converted = 0u;
            if (line[0] >= '0' && line[0] <= '9'
                    && parse_u32_from_csv_field(&p, &team_id)
                    && parse_u32_from_csv_field(&p, &league_id)
                    && parse_u32_from_csv_field(&p, &injured_player_id)
                    && parse_u32_from_csv_field(&p, &replacement_player_id)
                    && parse_u32_from_csv_field(&p, &opened_on)
                    && parse_u32_from_csv_field(&p, &expected_end)
                    && parse_u32_from_csv_field(&p, &slot_type)
                    && parse_u32_from_csv_field(&p, &status)
                    && parse_u32_from_csv_field(&p, &converted)
                    && team_id != 0u
                    && injured_player_id != 0u
                    && slot_type >= KBO_FOREIGN_INJURY_SLOT_REGULAR
                    && slot_type <= KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                    && status >= KBO_FOREIGN_INJURY_STATUS_OPEN
                    && status <= KBO_FOREIGN_INJURY_STATUS_CLOSED) {
                KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++];
                rec->team_id = team_id;
                rec->league_id = league_id;
                rec->injured_player_id = injured_player_id;
                rec->replacement_player_id = replacement_player_id;
                rec->opened_on_yyyymmdd = opened_on;
                rec->expected_end_yyyymmdd = expected_end;
                rec->slot_type = (uint8_t)slot_type;
                rec->status = (uint8_t)status;
                rec->converted = converted ? 1u : 0u;
                loaded++;
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    append_logf("foreign injury replacement: loaded=%d path=%s", loaded, path);
    return loaded;
}
