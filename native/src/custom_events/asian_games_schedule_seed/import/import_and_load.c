#include "../../runtime/common/custom_events_common.h"
#include "import_and_load.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

int kbo_import_asian_games_schedule_seed_file_locked(const char* path, const char* source)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 131072u) {
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

            char line[512] = {0};
            size_t line_len = (size_t)(next - cursor);
            while (line_len > 0u && (cursor[line_len - 1u] == '\r' || cursor[line_len - 1u] == '\n')) {
                line_len--;
            }
            if (line_len >= sizeof(line)) {
                line_len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, line_len);

            KboAsianGamesScheduleSeed seed;
            if (kbo_parse_asian_games_schedule_seed_line(line, &seed)) {
                kbo_add_asian_games_schedule_seed_locked(&seed);
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
            "KBO Asian Games schedule seed import source=%s imported=%d path=%s",
            source != NULL ? source : "",
            imported,
            path);
    }
    return imported;
}

void kbo_ensure_asian_games_schedule_seeds_loaded(void)
{
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    kbo_get_global_asian_games_schedule_seed_path(global_seed_path, sizeof(global_seed_path));
    kbo_get_save_asian_games_schedule_seed_path(save_seed_path, sizeof(save_seed_path));

    char loaded_key[MAX_PATH * 3] = {0};
    snprintf(loaded_key, sizeof(loaded_key), "%s|%s", global_seed_path, save_seed_path);

    kbo_lock_asian_games_schedule_seeds();
    if (InterlockedCompareExchange(&g_kbo_asian_games_schedule_seed_loaded, 1, 0) == 0
            || _stricmp(g_kbo_asian_games_schedule_seed_loaded_key, loaded_key) != 0) {
        memset(g_kbo_asian_games_schedule_seeds, 0, sizeof(g_kbo_asian_games_schedule_seeds));
        g_kbo_asian_games_schedule_seed_count = 0;
        snprintf(g_kbo_asian_games_schedule_seed_loaded_key, sizeof(g_kbo_asian_games_schedule_seed_loaded_key), "%s", loaded_key);
        kbo_add_builtin_asian_games_schedule_seeds_locked();
        kbo_import_asian_games_schedule_seed_file_locked(global_seed_path, "global_seed");
        kbo_import_asian_games_schedule_seed_file_locked(save_seed_path, "save_seed");
    }
    kbo_unlock_asian_games_schedule_seeds();
}
