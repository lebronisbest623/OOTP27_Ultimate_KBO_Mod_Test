#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/logging/core_log.h"

static int kbo_independent_acquisition_decision_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_ACQUISITION_DECISION_FILE,
        out,
        out_size);
}

static int kbo_independent_acquisition_json_u32(
    const char* line,
    const char* key,
    uint32_t* out)
{
    if (line == NULL || key == NULL || out == NULL) {
        return 0;
    }

    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    char* end = NULL;
    unsigned long value = strtoul(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int kbo_independent_acquisition_parse_decision_line(
    const char* line,
    uint32_t* out_season,
    uint32_t* out_seller_team_id,
    uint32_t* out_player_id,
    uint32_t* out_transferred)
{
    if (line == NULL) {
        return 0;
    }

    uint32_t season = 0u;
    uint32_t seller_team_id = 0u;
    uint32_t player_id = 0u;
    uint32_t transferred = 0u;
    if (!kbo_independent_acquisition_json_u32(line, "season", &season)
            || !kbo_independent_acquisition_json_u32(line, "seller_team_id", &seller_team_id)
            || !kbo_independent_acquisition_json_u32(line, "player_id", &player_id)) {
        return 0;
    }
    kbo_independent_acquisition_json_u32(line, "transferred", &transferred);

    if (out_season != NULL) { *out_season = season; }
    if (out_seller_team_id != NULL) { *out_seller_team_id = seller_team_id; }
    if (out_player_id != NULL) { *out_player_id = player_id; }
    if (out_transferred != NULL) { *out_transferred = transferred; }
    return season != 0u && seller_team_id != 0u && player_id != 0u;
}

int kbo_independent_acquisition_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int exists = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';

            uint32_t row_season = 0u;
            uint32_t row_seller_team_id = 0u;
            uint32_t row_player_id = 0u;
            if (kbo_independent_acquisition_parse_decision_line(
                    cursor,
                    &row_season,
                    &row_seller_team_id,
                    &row_player_id,
                    NULL)
                    && row_season == season
                    && row_seller_team_id == seller_team_id
                    && row_player_id == player_id) {
                exists = 1;
                *line_end = saved;
                break;
            }

            *line_end = saved;
            while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
                line_end++;
            }
            cursor = line_end;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return exists;
}

int kbo_independent_acquisition_transferred_count(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int count = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';

            uint32_t row_season = 0u;
            uint32_t row_seller_team_id = 0u;
            uint32_t transferred = 0u;
            if (kbo_independent_acquisition_parse_decision_line(
                    cursor,
                    &row_season,
                    &row_seller_team_id,
                    NULL,
                    &transferred)
                    && row_season == season
                    && row_seller_team_id == seller_team_id
                    && transferred != 0u) {
                count++;
            }

            *line_end = saved;
            while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
                line_end++;
            }
            cursor = line_end;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return count;
}

static void kbo_independent_acquisition_json_append_escaped(
    char* out,
    size_t out_size,
    const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (text == NULL) {
        text = "";
    }

    size_t used = strlen(out);
    for (const unsigned char* p = (const unsigned char*)text; *p != '\0' && used + 2u < out_size; p++) {
        unsigned char ch = *p;
        if (ch == '"' || ch == '\\') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = (char)ch;
        } else if (ch == '\r') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = 'r';
        } else if (ch == '\n') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = 'n';
        } else if (ch < 0x20u) {
            if (used + 7u >= out_size) { break; }
            int len = snprintf(out + used, out_size - used, "\\u%04x", (unsigned int)ch);
            if (len <= 0 || used + (size_t)len >= out_size) { break; }
            used += (size_t)len;
        } else {
            out[used++] = (char)ch;
        }
    }
    out[used] = '\0';
}

int kbo_independent_acquisition_append_decision(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int transferred,
    int32_t old_cash,
    int32_t new_cash,
    const char* source)
{
    if (today == 0u || request == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "independent acquisition seller AI decision skipped source=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            (unsigned long)GetLastError(),
            path);
        return 0;
    }

    char request_score_text[32] = {0};
    snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)request->request_score);

    char line[1024] = {0};
    snprintf(
        line,
        sizeof(line),
        "{\"date\":%u,\"season\":%u,\"seller_team_id\":%u,\"player_id\":%u,\"buyer_team_id\":%u,\"request_score\":%s,\"value_score\":%d,\"cash_cost\":%d,\"old_cash\":%d,\"new_cash\":%d,\"transferred\":%u,\"source\":\"",
        today,
        request->season,
        request->seller_team_id,
        request->player_id,
        request->buyer_team_id,
        request_score_text,
        request->value_score,
        request->cash_cost,
        old_cash,
        new_cash,
        transferred ? 1u : 0u);
    kbo_independent_acquisition_json_append_escaped(line, sizeof(line), source != NULL ? source : "");
    size_t used = strlen(line);
    snprintf(line + used, sizeof(line) - used, "\"}\r\n");

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(line);
    int ok = len > 0u && WriteFile(file, line, len, &written, NULL) && written == len;
    CloseHandle(file);
    return ok;
}
