#include "../internal/foreign_waiver_rights_internal.h"

/* Foreign reserve-right CSV loading. */

int kbo_load_foreign_waiver_rights(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD file_size = GetFileSize(file, NULL);
    if (file_size == INVALID_FILE_SIZE || file_size == 0) {
        CloseHandle(file);
        return 0;
    }
    char raw[65536] = {0};
    DWORD read = 0;
    if (file_size >= sizeof(raw)) {
        file_size = sizeof(raw) - 1u;
    }
    ReadFile(file, raw, file_size, &read, NULL);
    CloseHandle(file);
    if (read == 0u) {
        return 0;
    }
    raw[read] = '\0';

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    g_kbo_foreign_waiver_rights_count = 0;
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    char* cursor = raw;
    int deduped = 0;
    while (*cursor != '\0') {
        char* next = strchr(cursor, '\n');
        if (next == NULL) {
            next = cursor + strlen(cursor);
        }
        size_t len = (size_t)(next - cursor);
        while (len > 0 && (cursor[len - 1] == '\r' || cursor[len - 1] == '\n')) {
            len--;
        }
        if (len > 8) {
            char line[128] = {0};
            if (len >= sizeof(line)) {
                len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, len);
            const char* p = line;
            if (line[0] != '#' && line[0] != ';') {
                uint32_t player_id = 0;
                uint32_t team_id = 0;
                uint32_t league_id = 0;
                uint32_t retained_on = 0;
                uint32_t expires_on = 0;
                if (parse_u32_from_csv_field((const char**)&p, &player_id)
                    && parse_u32_from_csv_field((const char**)&p, &team_id)
                    && parse_u32_from_csv_field((const char**)&p, &league_id)
                    && parse_u32_from_csv_field((const char**)&p, &retained_on)
                    && parse_u32_from_csv_field((const char**)&p, &expires_on)) {
                    if (player_id != 0u && team_id != 0u && league_id != 0u) {
                        while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
                            Sleep(0);
                        }
                        int existing_index = -1;
                        for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
                            if (g_kbo_foreign_waiver_rights[i].player_id == player_id) {
                                existing_index = i;
                                break;
                            }
                        }
                        if (existing_index >= 0) {
                            KboForeignWaiverRetention* existing = &g_kbo_foreign_waiver_rights[existing_index];
                            if (retained_on >= existing->retained_on_yyyymmdd) {
                                *existing = (KboForeignWaiverRetention){
                                    player_id,
                                    team_id,
                                    league_id,
                                    retained_on,
                                    expires_on
                                };
                            }
                            deduped++;
                        } else if (g_kbo_foreign_waiver_rights_count < KBO_FOREIGN_WAIVER_RIGHTS_MAX) {
                            g_kbo_foreign_waiver_rights[g_kbo_foreign_waiver_rights_count++] = (KboForeignWaiverRetention){
                                player_id,
                                team_id,
                                league_id,
                                retained_on,
                                expires_on
                            };
                        }
                        InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
                    }
                }
            }
        }
        if (*next == '\0') {
            break;
        }
        cursor = next + 1;
    }
    append_logf("foreign reserve rights: loaded=%d deduped=%d path=%s", g_kbo_foreign_waiver_rights_count, deduped, path);
    if (deduped > 0) {
        kbo_persist_foreign_waiver_rights();
    }
    return 1;
}

