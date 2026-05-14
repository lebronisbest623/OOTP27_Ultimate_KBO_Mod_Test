#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/csv/core_csv.h"
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

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int found = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[10][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 10);
        if (field_count < 7 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        uint32_t row_end = kbo_csv_parse_u32_text(fields[2], 10);
        uint32_t row_team = kbo_csv_parse_u32_text(fields[5], 10);
        uint32_t row_player = kbo_csv_parse_u32_text(fields[6], 10);
        if (row_end == window_end && row_team == team_id && row_player == player_id) {
            found = 1;
            break;
        }
    }

    kbo_csv_reader_close(reader);
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

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int found = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[10][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 10);
        if (field_count < 7 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        uint32_t row_end = kbo_csv_parse_u32_text(fields[2], 10);
        uint32_t row_team = kbo_csv_parse_u32_text(fields[5], 10);
        uint32_t row_player = kbo_csv_parse_u32_text(fields[6], 10);
        if (row_end == window_end && row_team == team_id && row_player == player_id) {
            snprintf(out_action, out_action_size, "%s", fields[4]);
            found = 1;
        }
    }

    kbo_csv_reader_close(reader);
    return found;
}

