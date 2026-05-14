#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/injury/api/foreign_injury_labels.h"

static int kbo_independent_acquisition_request_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_ACQUISITION_REQUEST_FILE,
        out,
        out_size);
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

int kbo_independent_acquisition_request_exists(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_request_path(path, sizeof(path))) {
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
        char season_token[40] = {0};
        char buyer_token[48] = {0};
        char seller_token[48] = {0};
        char player_token[48] = {0};
        snprintf(season_token, sizeof(season_token), "\"season\":%u", season);
        snprintf(buyer_token, sizeof(buyer_token), "\"buyer_team_id\":%u", buyer_team_id);
        snprintf(seller_token, sizeof(seller_token), "\"seller_team_id\":%u", seller_team_id);
        snprintf(player_token, sizeof(player_token), "\"player_id\":%u", player_id);
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';
            if (strstr(cursor, season_token) != NULL
                    && strstr(cursor, buyer_token) != NULL
                    && strstr(cursor, seller_token) != NULL
                    && strstr(cursor, player_token) != NULL) {
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

static int kbo_independent_acquisition_json_i64(
    const char* line,
    const char* key,
    int64_t* out)
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
    long long value = _strtoi64(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (int64_t)value;
    return 1;
}

static int kbo_independent_acquisition_parse_request_line(
    const char* line,
    KboIndependentAcquisitionQueuedRequest* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }

    KboIndependentAcquisitionQueuedRequest row;
    memset(&row, 0, sizeof(row));
    int64_t request_score = INT64_MIN;
    uint32_t value_score = 0u;
    uint32_t cash_cost = 0u;
    if (!kbo_independent_acquisition_json_u32(line, "date", &row.date)
            || !kbo_independent_acquisition_json_u32(line, "season", &row.season)
            || !kbo_independent_acquisition_json_u32(line, "buyer_team_id", &row.buyer_team_id)
            || !kbo_independent_acquisition_json_u32(line, "seller_team_id", &row.seller_team_id)
            || !kbo_independent_acquisition_json_u32(line, "player_id", &row.player_id)
            || !kbo_independent_acquisition_json_i64(line, "request_score", &request_score)) {
        return 0;
    }
    kbo_independent_acquisition_json_u32(line, "value_score", &value_score);
    kbo_independent_acquisition_json_u32(line, "cash_cost", &cash_cost);
    row.request_score = request_score;
    row.value_score = (int32_t)value_score;
    row.cash_cost = (int32_t)cash_cost;
    *out = row;
    return row.season != 0u
        && row.buyer_team_id != 0u
        && row.seller_team_id != 0u
        && row.player_id != 0u;
}

int kbo_independent_acquisition_cancel_request(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id,
    const char* source)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }
    if (kbo_independent_acquisition_decision_exists(season, seller_team_id, player_id)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_request_path(path, sizeof(path))) {
        return 0;
    }
    char temp_path[MAX_PATH] = {0};
    if (strlen(path) + 4u >= sizeof(temp_path)) {
        return 0;
    }
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

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
    int removed = 0;
    int write_ok = 1;
    if (!ReadFile(file, buffer, size, &read, NULL) || read == 0u) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return 0;
    }
    buffer[read] = '\0';
    CloseHandle(file);

    HANDLE temp = CreateFileA(
        temp_path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (temp == INVALID_HANDLE_VALUE) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }

    char* cursor = buffer;
    char* end = buffer + read;
    while (cursor < end) {
        char* content_end = cursor;
        while (content_end < end && *content_end != '\r' && *content_end != '\n') {
            content_end++;
        }
        char* next = content_end;
        while (next < end && (*next == '\r' || *next == '\n')) {
            next++;
        }

        char saved = *content_end;
        *content_end = '\0';
        KboIndependentAcquisitionQueuedRequest row;
        int remove_line =
            kbo_independent_acquisition_parse_request_line(cursor, &row)
            && row.season == season
            && row.buyer_team_id == buyer_team_id
            && row.seller_team_id == seller_team_id
            && row.player_id == player_id;
        *content_end = saved;

        if (remove_line) {
            removed++;
        } else {
            DWORD to_write = (DWORD)(next - cursor);
            DWORD written = 0u;
            if (to_write > 0u
                    && (!WriteFile(temp, cursor, to_write, &written, NULL) || written != to_write)) {
                write_ok = 0;
                break;
            }
        }
        cursor = next;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(temp);

    if (!write_ok) {
        DeleteFileA(temp_path);
        return 0;
    }
    if (removed <= 0) {
        DeleteFileA(temp_path);
        return 0;
    }
    if (!MoveFileExA(temp_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        kbo_log_runtimef(
            "independent acquisition request cancel failed source=%s reason=replace_failed gle=%lu path=%s",
            source != NULL ? source : "",
            (unsigned long)GetLastError(),
            path);
        DeleteFileA(temp_path);
        return 0;
    }

    kbo_log_runtimef(
        "independent acquisition request cancelled source=%s season=%u buyer=%u seller=%u player=%u removed=%d",
        source != NULL ? source : "",
        season,
        buyer_team_id,
        seller_team_id,
        player_id,
        removed);
    return removed;
}

int kbo_independent_acquisition_append_request(
    uint32_t today,
    const KboIndependentAcquisitionCandidate* candidate,
    const KboIndependentAcquisitionBuyerState* buyer,
    const KboIndependentFuturesTeamLeague* seller,
    const char* source)
{
    if (today == 0u || candidate == NULL || buyer == NULL || seller == NULL) {
        return 0;
    }
    if (kbo_independent_acquisition_request_exists(
            today / 10000u,
            buyer->team_id,
            candidate->seller_team_id,
            candidate->player_id)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_request_path(path, sizeof(path))) {
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
            "independent acquisition AI request skipped source=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            (unsigned long)GetLastError(),
            path);
        return 0;
    }

    char request_score_text[32] = {0};
    snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)candidate->request_score);
    int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player((uint8_t*)candidate->player_ptr);

    char line[1536] = {0};
    snprintf(
        line,
        sizeof(line),
        "{\"date\":%u,\"season\":%u,\"buyer_team_id\":%u,\"seller_team_id\":%u,\"seller_csv_id\":\"",
        today,
        today / 10000u,
        buyer->team_id,
        candidate->seller_team_id);
    kbo_independent_acquisition_json_append_escaped(line, sizeof(line), seller->team_csv_id);
    size_t used = strlen(line);
    snprintf(
        line + used,
        sizeof(line) - used,
        "\",\"player_id\":%u,\"nation_id\":%u,\"pitcher\":%u,\"asian_quota\":%u,\"cash_cost\":%d,\"value_score\":%d,\"request_score\":%s,\"effective_before\":%u,\"effective_after\":%u,\"effective_limit\":%u,\"slot_type\":\"%s\",\"injured_player_id\":%u,\"buyer_active_count\":%u,\"buyer_foreign_effective\":%u,\"source\":\"",
        candidate->player_id,
        candidate->nation_id,
        (uint32_t)candidate->pitcher,
        (uint32_t)candidate->asian_quota,
        cash_cost,
        candidate->value_score,
        request_score_text,
        candidate->effective_before,
        candidate->effective_after,
        candidate->effective_limit,
        candidate->slot_type != 0u ? kbo_foreign_injury_slot_label(candidate->slot_type) : "none",
        candidate->injured_player_id,
        buyer->active_count,
        buyer->effective_foreign_count);
    kbo_independent_acquisition_json_append_escaped(line, sizeof(line), source != NULL ? source : "");
    used = strlen(line);
    snprintf(line + used, sizeof(line) - used, "\"}\r\n");

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(line);
    int ok = len > 0u && WriteFile(file, line, len, &written, NULL) && written == len;
    CloseHandle(file);
    return ok;
}

int kbo_independent_acquisition_load_requests(
    uint32_t season,
    KboIndependentAcquisitionQueuedRequest* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_request_path(path, sizeof(path))) {
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
        while (cursor < end && count < max_count) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';

            KboIndependentAcquisitionQueuedRequest row;
            if (kbo_independent_acquisition_parse_request_line(cursor, &row)
                    && row.season == season
                    && !kbo_independent_acquisition_decision_exists(row.season, row.seller_team_id, row.player_id)) {
                out[count++] = row;
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
