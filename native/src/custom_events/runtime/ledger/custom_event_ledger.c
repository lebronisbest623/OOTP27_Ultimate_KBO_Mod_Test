#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "custom_event_ledger.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"

#define KBO_CUSTOM_EVENT_LEDGER_FILE "custom_event_runs.jsonl"
#define KBO_CUSTOM_EVENT_LEDGER_MAX_READ (4u * 1024u * 1024u)

int kbo_custom_event_ledger_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(KBO_CUSTOM_EVENT_LEDGER_FILE, out, out_size);
}

static int kbo_custom_event_ledger_key(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u || event_yyyymmdd == 0u
            || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }
    int len = snprintf(
        out,
        out_size,
        "%s|%u|%u",
        kbo_custom_event_kind_key(kind),
        league_id,
        event_yyyymmdd);
    return len > 0 && len < (int)out_size;
}

static void kbo_custom_event_json_append_raw(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0u || text == NULL) {
        return;
    }
    size_t used = strlen(out);
    if (used >= out_size) {
        return;
    }
    snprintf(out + used, out_size - used, "%s", text);
}

static void kbo_custom_event_json_append_escaped(char* out, size_t out_size, const char* text)
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
            if (used + 3u >= out_size) {
                break;
            }
            out[used++] = '\\';
            out[used++] = (char)ch;
        } else if (ch == '\r') {
            if (used + 3u >= out_size) {
                break;
            }
            out[used++] = '\\';
            out[used++] = 'r';
        } else if (ch == '\n') {
            if (used + 3u >= out_size) {
                break;
            }
            out[used++] = '\\';
            out[used++] = 'n';
        } else if (ch < 0x20u) {
            if (used + 7u >= out_size) {
                break;
            }
            int len = snprintf(out + used, out_size - used, "\\u%04x", (unsigned int)ch);
            if (len <= 0 || used + (size_t)len >= out_size) {
                break;
            }
            used += (size_t)len;
        } else {
            out[used++] = (char)ch;
        }
    }
    out[used] = '\0';
}

int kbo_custom_event_ledger_completed(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind)
{
    char key[96] = {0};
    if (!kbo_custom_event_ledger_key(league_id, event_yyyymmdd, kind, key, sizeof(key))) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_custom_event_ledger_path(path, sizeof(path))) {
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
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > KBO_CUSTOM_EVENT_LEDGER_MAX_READ) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int found = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';

        char key_token[128] = {0};
        snprintf(key_token, sizeof(key_token), "\"event_key\":\"%s\"", key);
        const char* status_token = "\"status\":\"completed\"";

        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';
            if (strstr(cursor, key_token) != NULL && strstr(cursor, status_token) != NULL) {
                found = 1;
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
    return found;
}

void kbo_custom_event_ledger_record(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source)
{
    char key[96] = {0};
    if (!kbo_custom_event_ledger_key(league_id, event_yyyymmdd, kind, key, sizeof(key))
            || status == NULL || status[0] == '\0') {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_custom_event_ledger_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "KBO custom event ledger skipped source=%s kind=%s date=%u reason=path_unavailable",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_yyyymmdd);
        return;
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
            "KBO custom event ledger skipped source=%s kind=%s date=%u reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_yyyymmdd,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);

    char line[2048] = {0};
    snprintf(
        line,
        sizeof(line),
        "{\"ts\":\"%04u-%02u-%02uT%02u:%02u:%02u\",\"event_key\":\"%s\",\"kind\":\"%s\",\"league_id\":%u,\"event_date\":%u,\"status\":\"",
        (unsigned int)now.wYear,
        (unsigned int)now.wMonth,
        (unsigned int)now.wDay,
        (unsigned int)now.wHour,
        (unsigned int)now.wMinute,
        (unsigned int)now.wSecond,
        key,
        kbo_custom_event_kind_key(kind),
        league_id,
        event_yyyymmdd);
    kbo_custom_event_json_append_escaped(line, sizeof(line), status);
    kbo_custom_event_json_append_raw(line, sizeof(line), "\",\"result\":");

    char result_text[32] = {0};
    snprintf(result_text, sizeof(result_text), "%d", result);
    kbo_custom_event_json_append_raw(line, sizeof(line), result_text);

    kbo_custom_event_json_append_raw(line, sizeof(line), ",\"title\":\"");
    kbo_custom_event_json_append_escaped(line, sizeof(line), title);
    kbo_custom_event_json_append_raw(line, sizeof(line), "\",\"detail\":\"");
    kbo_custom_event_json_append_escaped(line, sizeof(line), detail);
    kbo_custom_event_json_append_raw(line, sizeof(line), "\",\"source\":\"");
    kbo_custom_event_json_append_escaped(line, sizeof(line), source);
    kbo_custom_event_json_append_raw(line, sizeof(line), "\"}\r\n");

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(line);
    if (len == 0u || !WriteFile(file, line, len, &written, NULL) || written != len) {
        kbo_log_runtimef(
            "KBO custom event ledger write failed source=%s kind=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_yyyymmdd,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}
