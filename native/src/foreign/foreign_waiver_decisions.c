#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../bootstrap/ootp_offsets.h"
#include "../core/core_flags/flags_api.h"
#include "../core/core_log.h"
#include "../core/core_save_paths.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/team_lookup.h"
#include "../team/team_roster_arrays.h"
#include "foreign_csv_parse.h"
#include "foreign_waiver_core.h"
#include "foreign_waiver_date.h"
#include "foreign_waiver_decisions.h"
#include "foreign_waiver_paths.h"
#include "foreign_waiver_player_eval.h"
#include "foreign_waiver_policy.h"
#include "rights/foreign_waiver_rights_query.h"

static LONG g_kbo_foreign_waiver_decision_lock = 0;
/* ---- native/src/foreign/foreign_decision_team.inc ---- */
/* Foreign reserve-right decision-team and original-club priority helpers. Included from native/src/foreign_waiver_ai.inc. */

uint32_t kbo_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
}

uint32_t kbo_get_foreign_waiver_decision_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    /*
     * OOTP saves can carry an active_team_id for foreign players at league start.
     * That is only the club with priority to decide during the KBO reserve-rights
     * event, not an already exercised reserve right. Stored rights are handled by
     * foreign_waiver_rights.csv; candidate ownership should appear only while the
     * negotiation window is open.
     */
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return 0;
    }

    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (active_team_id != 0u) {
        return active_team_id;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (current_team_id != 0u) {
        return current_team_id;
    }

    return 0u;
}

static int kbo_original_club_priority_window_allows(uint8_t* player, uint32_t team_id, const char* action_name)
{
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return 1;
    }

    uint32_t priority_team_id = kbo_get_foreign_waiver_decision_team_id(player);
    if (priority_team_id == 0 || team_id == priority_team_id) {
        return 1;
    }

    uint32_t player_id = 0;
    if (player != NULL && memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    }
    append_logf(
        "foreign priority negotiation: blocked action=%s team=%u player=%u priority_team=%u",
        action_name == NULL ? "" : action_name,
        team_id,
        player_id,
        priority_team_id);
    return 0;
}

/* ---- native/src/foreign/foreign_waiver_retain.inc ---- */
/* Foreign reserve-right retention mutation. Included from native/src/foreign_waiver_ai.inc. */

int kbo_retain_foreign_player_rights(
    uint8_t* player,
    uint8_t* retaining_team,
    uint32_t fallback_league_id,
    uint32_t player_id,
    uint32_t team_id)
{
    if (player == NULL || retaining_team == NULL || player_id == 0 || team_id == 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(retaining_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t team_numeric_id = *(uint32_t*)(retaining_team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (team_numeric_id != team_id) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(retaining_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == 0) {
        team_league_id = fallback_league_id;
    }
    if (team_league_id == 0) {
        return 0;
    }

    uint32_t today_yyyymmdd = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today_yyyymmdd)) {
        return 0;
    }
    uint32_t expires_yyyymmdd = kbo_add_years_yyyymmdd(today_yyyymmdd, KBO_FOREIGN_WAIVER_RETENTION_YEARS);
    if (expires_yyyymmdd == 0u) {
        return 0;
    }

    if (!kbo_set_foreign_waiver_right(team_id, player_id, team_league_id, today_yyyymmdd, expires_yyyymmdd)) {
        append_logf("foreign reserve rights: failed to store right team=%u player=%u", team_id, player_id);
        return 0;
    }

    if (!kbo_add_player_id_to_team_fixed_array(retaining_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)) {
        append_logf("foreign reserve rights: restricted array full team=%u player=%u", team_id, player_id);
        return 0;
    }

    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1;
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = team_id;
    *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = team_league_id;

    append_logf("foreign reserve rights: retained team=%u player=%u league=%u from=%u until=%u",
               team_id, player_id, team_league_id, today_yyyymmdd, expires_yyyymmdd);
    return 1;
}

/* ---- native/src/foreign/foreign_waiver_decisions.inc ---- */
/* Foreign reserve-right decision record IO. Included from native/src/foreign_waiver_ai.inc. */

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

/* ---- native/src/foreign/foreign_waiver_command_execute.inc ---- */
/* Foreign reserve-right command execution. Included from native/src/foreign_waiver_ai.inc. */

static int kbo_execute_foreign_waiver_claim(const char* line, int line_no)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    if (line[0] == '#' || line[0] == ';') {
        return 0;
    }

    char raw[512];
    size_t raw_len = 0;
    for (; raw_len < sizeof(raw) - 1 && line[raw_len] != '\0' && line[raw_len] != '\r' && line[raw_len] != '\n'; raw_len++) {
        raw[raw_len] = line[raw_len];
    }
    raw[raw_len] = '\0';

    const char* p = raw;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    char action_name[16] = {0};
    size_t idx = 0;
    while (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t' && idx + 1 < sizeof(action_name)) {
        action_name[idx++] = *p++;
    }
    action_name[idx] = '\0';

    int retain_rights =
        strcmp(action_name, "RETAIN") == 0 || strcmp(action_name, "retain") == 0
        || strcmp(action_name, "RIGHTS") == 0 || strcmp(action_name, "rights") == 0
        || strcmp(action_name, "RESERVE") == 0 || strcmp(action_name, "reserve") == 0;
    int skip_rights =
        strcmp(action_name, "SKIP") == 0 || strcmp(action_name, "skip") == 0;

    if (strcmp(action_name, "CLAIM") != 0 && strcmp(action_name, "claim") != 0
            && strcmp(action_name, "MOVE") != 0 && strcmp(action_name, "move") != 0
            && !retain_rights && !skip_rights) {
        return 0;
    }

    if (*p == ',') {
        p++;
    } else if (*p != '\0') {
        while (*p != '\0' && *p != ',') {
            p++;
        }
        if (*p == ',') {
            p++;
        }
    }

    uint32_t team_id = 0;
    uint32_t player_id = 0;
    if (!parse_u32_from_csv_field(&p, &team_id) || !parse_u32_from_csv_field(&p, &player_id)) {
        append_logf("foreign waiver command line %d malformed: %s", line_no, raw);
        return 0;
    }

    if (team_id == 0 || player_id == 0) {
        append_logf("foreign waiver command line %d ignored (zero id): %s", line_no, raw);
        return 0;
    }

    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (destination_team == NULL) {
        append_logf("foreign waiver command line %d failed: team=%u not found", line_no, team_id);
        return 0;
    }

    uint32_t player_current_team = 0;
    uint32_t player_current_league = 0;
    uint8_t* player = kbo_find_player_by_id(player_id, &player_current_team, &player_current_league);
    if (player == NULL) {
        append_logf("foreign waiver command line %d failed: player=%u not found", line_no, player_id);
        return 0;
    }

    if (!kbo_original_club_priority_window_allows(player, team_id, action_name)) {
        return 0;
    }

    if (retain_rights) {
        uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t fallback_league = player_current_league != 0 ? player_current_league : destination_league;
        if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, team_id)) {
            append_logf("foreign waiver command line %d failed: action=%s team=%u player=%u",
                       line_no, action_name, team_id, player_id);
            return 0;
        }

        append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        kbo_append_foreign_waiver_decision_record("user", "RETAIN", team_id, player_id, 0, 0, 1);
        return 1;
    }

    if (skip_rights) {
        kbo_clear_foreign_waiver_right(team_id, player_id);
        append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        kbo_append_foreign_waiver_decision_record("user", "SKIP", team_id, player_id, 0, 0, 1);
        return 1;
    }

    if (player_current_team == team_id) {
        append_logf("foreign waiver command line %d no-op: player=%u already on team=%u", line_no, player_id, team_id);
        return 1;
    }

    uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t fallback_league = player_current_league != 0 ? player_current_league : destination_league;
    if (fallback_league == 0) {
        return 0;
    }

    if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, team_id)) {
        append_logf("foreign waiver command line %d failed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        return 0;
    }
    append_logf("foreign waiver command line %d executed: action=%s team=%u player=%u",
               line_no, action_name, team_id, player_id);
    kbo_append_foreign_waiver_decision_record("user", "RETAIN", team_id, player_id, 0, 0, 1);
    return 1;
}

static int get_kbo_foreign_waiver_cmd_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_commands.txt", out, out_size);
}

static int kbo_append_foreign_waiver_cmd_line(const char* line)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_cmd_path(path, sizeof(path))) {
        return 0;
    }
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_decision_lock, 1, 0) != 0) {
        Sleep(0);
    }
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
        return 0;
    }
    DWORD wrote = 0;
    char out[128] = {0};
    int len = snprintf(out, sizeof(out), "%s\r\n", line);
    WriteFile(file, out, (DWORD)len, &wrote, NULL);
    CloseHandle(file);
    InterlockedExchange(&g_kbo_foreign_waiver_decision_lock, 0);
    return wrote == (DWORD)len;
}

int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        append_logf("foreign waiver decision: blocked by window state team=%u player=%u action=%s", team_id, player_id, retain ? "RETAIN" : "SKIP");
        return 0;
    }
    char line[128] = {0};
    int len = snprintf(line, sizeof(line), "%s,%u,%u", retain ? "RETAIN" : "SKIP", team_id, player_id);
    if (len <= 0) {
        return 0;
    }
    return kbo_append_foreign_waiver_cmd_line(line);
}
void process_foreign_waiver_commands(void)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_cmd_path(path, sizeof(path))) {
        append_log_line("foreign waiver command: unable to resolve command path");
        return;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD file_size_low = GetFileSize(file, NULL);
    if (file_size_low == INVALID_FILE_SIZE || file_size_low == 0) {
        CloseHandle(file);
        return;
    }
    if (file_size_low >= 30000) {
        CloseHandle(file);
        append_log_line("foreign waiver command: command file too large; skip to avoid blocking");
        return;
    }

    char input[30000] = {0};
    DWORD read = 0;
    if (!ReadFile(file, input, file_size_low, &read, NULL) || read == 0) {
        CloseHandle(file);
        return;
    }
    CloseHandle(file);
    input[read] = '\0';

    char remaining[30000] = {0};
    char* keep = remaining;
    DWORD remain_len = 0;
    DWORD used_commands = 0;
    DWORD executed_commands = 0;

    const char* cursor = input;
    int line_no = 1;
    while (cursor < input + read) {
        const char* line_end = strchr(cursor, '\n');
        if (line_end == NULL) {
            line_end = input + read;
        }

        size_t len = (size_t)(line_end - cursor);
        while (len > 0 && (cursor[len - 1] == '\r' || cursor[len - 1] == '\n')) {
            len--;
        }

        char line[512] = {0};
        if (len >= sizeof(line)) {
            len = sizeof(line) - 1;
        }
        memcpy(line, cursor, len);
        line[len] = '\0';

        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') {
            trimmed++;
        }

        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            if (remain_len + len + 1 < sizeof(remaining)) {
                size_t add_len = strlen(line);
                memcpy(keep + remain_len, line, add_len);
                keep[remain_len + add_len] = '\r';
                keep[remain_len + add_len + 1] = '\n';
                remain_len += (DWORD)(add_len + 2);
            }
        } else {
            used_commands++;
            if (!kbo_execute_foreign_waiver_claim(trimmed, line_no)) {
                if (remain_len + len + 2 < sizeof(remaining)) {
                    size_t add_len = strlen(trimmed);
                    memcpy(keep + remain_len, trimmed, add_len);
                    keep[remain_len + add_len] = '\r';
                    keep[remain_len + add_len + 1] = '\n';
                    remain_len += (DWORD)(add_len + 2);
                }
            } else {
                executed_commands++;
            }
        }

        if (*line_end == '\0') {
            break;
        }
        cursor = line_end + 1;
        line_no++;
    }

    if (used_commands > 0) {
        file = CreateFileA(
            path,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file != INVALID_HANDLE_VALUE) {
            if (remain_len > 0) {
                DWORD written = 0;
                WriteFile(file, remaining, remain_len, &written, NULL);
            }
            CloseHandle(file);
        }
        append_logf("foreign waiver command: processed=%lu executed=%lu keep_len=%lu", used_commands, executed_commands, remain_len);
        return;
    }
}
