#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../common/csv/foreign_csv_parse.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/paths/foreign_waiver_paths.h"
#include "../../waiver_core/api/foreign_waiver_core.h"
#include "../api/foreign_waiver_decisions.h"
#include "../internal/foreign_waiver_decisions_state_internal.h"

int kbo_append_foreign_waiver_decision_record(
    const char* source,
    const char* action,
    uint32_t team_id,
    uint32_t player_id,
    int score,
    int forced,
    int executed)
{
    if (source == NULL || source[0] == '\0' || action == NULL || action[0] == '\0'
            || team_id == 0u || player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_decisions_path(path, sizeof(path))) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);

    uint32_t window_start = 0u;
    uint32_t window_end = 0u;
    kbo_current_foreign_waiver_window_dates(&window_start, &window_end);

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_decision_lock, 1, 0) != 0) {
        Sleep(0);
    }

    DWORD attrs = GetFileAttributesA(path);
    int needs_header = (attrs == INVALID_FILE_ATTRIBUTES);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
        return 0;
    }

    DWORD written = 0;
    if (needs_header) {
        const char* header = "decision_date,window_start,window_end,source,action,team_id,player_id,value_score,forced,executed\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char line[256] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%u,%u,%u,%s,%s,%u,%u,%d,%d,%d\r\n",
        today,
        window_start,
        window_end,
        source,
        action,
        team_id,
        player_id,
        score,
        forced ? 1 : 0,
        executed ? 1 : 0);
    int ok = 0;
    if (len > 0 && len < (int)sizeof(line)) {
        written = 0;
        ok = WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len;
    }

    CloseHandle(file);
    InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
    return ok;
}

int kbo_foreign_waiver_decision_exists(uint32_t window_end, uint32_t team_id, uint32_t player_id)
{
    if (window_end == 0u || team_id == 0u || player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_decisions_path(path, sizeof(path))) {
        return 0;
    }

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
    int found = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
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
            uint32_t decision_date = 0u;
            uint32_t row_start = 0u;
            uint32_t row_end = 0u;
            uint32_t row_team = 0u;
            uint32_t row_player = 0u;
            if (line[0] >= '0' && line[0] <= '9'
                    && parse_u32_from_csv_field(&p, &decision_date)
                    && parse_u32_from_csv_field(&p, &row_start)
                    && parse_u32_from_csv_field(&p, &row_end)) {
                for (int comma = 0; comma < 3 && *p != '\0'; comma++) {
                    while (*p != '\0' && *p != ',') { p++; }
                    if (*p == ',') { p++; }
                }
                if (parse_u32_from_csv_field(&p, &row_team)
                        && parse_u32_from_csv_field(&p, &row_player)
                        && row_end == window_end
                        && row_team == team_id
                        && row_player == player_id) {
                    found = 1;
                    break;
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
    return found;
}

int kbo_foreign_waiver_latest_decision_action(
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id,
    char* out_action,
    size_t out_action_size)
{
    if (out_action != NULL && out_action_size > 0u) {
        out_action[0] = '\0';
    }
    if (window_end == 0u || team_id == 0u || player_id == 0u || out_action == NULL || out_action_size == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_decisions_path(path, sizeof(path))) {
        return 0;
    }

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
    int found = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0') {
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
            uint32_t decision_date = 0u;
            uint32_t row_start = 0u;
            uint32_t row_end = 0u;
            if (line[0] >= '0' && line[0] <= '9'
                    && parse_u32_from_csv_field(&p, &decision_date)
                    && parse_u32_from_csv_field(&p, &row_start)
                    && parse_u32_from_csv_field(&p, &row_end)
                    && row_end == window_end) {
                while (*p == ',' || *p == ' ' || *p == '\t') { p++; }
                while (*p != '\0' && *p != ',') { p++; }
                if (*p == ',') { p++; }

                char action_name[16] = {0};
                while (*p == ' ' || *p == '\t') { p++; }
                size_t action_len = 0u;
                while (*p != '\0' && *p != ',' && action_len + 1u < sizeof(action_name)) {
                    action_name[action_len++] = *p++;
                }
                action_name[action_len] = '\0';
                if (*p == ',') { p++; }

                uint32_t row_team = 0u;
                uint32_t row_player = 0u;
                if (parse_u32_from_csv_field(&p, &row_team)
                        && parse_u32_from_csv_field(&p, &row_player)
                        && row_team == team_id
                        && row_player == player_id) {
                    snprintf(out_action, out_action_size, "%s", action_name);
                    found = 1;
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
    return found;
}

