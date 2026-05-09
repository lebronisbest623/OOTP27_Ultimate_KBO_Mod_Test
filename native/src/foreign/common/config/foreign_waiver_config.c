#include "foreign_waiver_config.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

uint32_t read_u32_leading_number_from_file(const char* filename)
{
    if (filename == NULL) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\%s", local_app_data, filename);

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char buf[128] = {0};
    DWORD read = 0;
    if (!ReadFile(file, buf, sizeof(buf) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);

    const char* cursor = buf;
    while (*cursor == ' ' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t' || *cursor == ',') {
        cursor++;
    }
    if (*cursor == '\0') {
        return 0;
    }

    char* tail = NULL;
    unsigned long long raw = strtoull(cursor, &tail, 10);
    if (raw == 0ULL || raw > UINT32_MAX) {
        return 0;
    }
    return (uint32_t)raw;
}

uint32_t kbo_get_foreign_waiver_auto_target_team_id(void)
{
    return read_u32_leading_number_from_file("foreign_waiver_ai_targets.txt");
}

int kbo_is_forced_foreign_candidate_id(uint32_t player_id)
{
    if (player_id == 0) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\foreign_player_ids.txt", local_app_data);

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char buf[4096] = {0};
    DWORD read = 0;
    if (!ReadFile(file, buf, sizeof(buf) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);

    const char* cursor = buf;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t' || *cursor == ',') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        char* tail = NULL;
        unsigned long long raw = strtoull(cursor, &tail, 10);
        if (raw == 0ULL && cursor == tail) {
            break;
        }

        if (raw == (unsigned long long)player_id) {
            return 1;
        }

        cursor = tail;
        while (*cursor != '\0' && *cursor != ',' && *cursor != '\n' && *cursor != '\r') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        while (*cursor == ',' || *cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
    }

    return 0;
}
