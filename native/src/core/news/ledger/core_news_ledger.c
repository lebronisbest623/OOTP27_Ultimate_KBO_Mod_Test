#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core_news_ledger.h"

#include <stdio.h>
#include <string.h>

#include "../../files/save_paths/core_save_paths.h"
#include "../../logging/core_log.h"

#define KBO_CUSTOM_NEWS_LEDGER_FILE "custom_news_runs.jsonl"
#define KBO_CUSTOM_NEWS_LEDGER_MAX_READ (4u * 1024u * 1024u)

int kbo_custom_news_ledger_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(KBO_CUSTOM_NEWS_LEDGER_FILE, out, out_size);
}

static int kbo_custom_news_ledger_key(
    const char* domain,
    const char* key,
    char* out,
    size_t out_size)
{
    if (domain == NULL || domain[0] == '\0' || key == NULL || key[0] == '\0'
            || out == NULL || out_size == 0u) {
        return 0;
    }
    int len = snprintf(out, out_size, "%s|%s", domain, key);
    return len > 0 && len < (int)out_size;
}

static void kbo_custom_news_json_append_raw(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0u || text == NULL) {
        return;
    }
    size_t used = strlen(out);
    if (used < out_size) {
        snprintf(out + used, out_size - used, "%s", text);
    }
}

static void kbo_custom_news_json_append_escaped(char* out, size_t out_size, const char* text)
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

int kbo_custom_news_ledger_completed(const char* domain, const char* key)
{
    char ledger_key[256] = {0};
    if (!kbo_custom_news_ledger_key(domain, key, ledger_key, sizeof(ledger_key))) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_custom_news_ledger_path(path, sizeof(path))) {
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
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > KBO_CUSTOM_NEWS_LEDGER_MAX_READ) {
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

        char key_token[320] = {0};
        snprintf(key_token, sizeof(key_token), "\"news_key\":\"%s\"", ledger_key);
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

void kbo_custom_news_ledger_record(
    const char* domain,
    const char* key,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source)
{
    char ledger_key[256] = {0};
    if (!kbo_custom_news_ledger_key(domain, key, ledger_key, sizeof(ledger_key))
            || status == NULL || status[0] == '\0') {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_custom_news_ledger_path(path, sizeof(path))) {
        append_logf(
            "KBO custom news ledger skipped source=%s domain=%s key=%s reason=path_unavailable",
            source != NULL ? source : "",
            domain != NULL ? domain : "",
            key != NULL ? key : "");
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
        append_logf(
            "KBO custom news ledger skipped source=%s domain=%s key=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            domain != NULL ? domain : "",
            key != NULL ? key : "",
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
        "{\"ts\":\"%04u-%02u-%02uT%02u:%02u:%02u\",\"news_key\":\"%s\",\"domain\":\"",
        (unsigned int)now.wYear,
        (unsigned int)now.wMonth,
        (unsigned int)now.wDay,
        (unsigned int)now.wHour,
        (unsigned int)now.wMinute,
        (unsigned int)now.wSecond,
        ledger_key);
    kbo_custom_news_json_append_escaped(line, sizeof(line), domain);
    kbo_custom_news_json_append_raw(line, sizeof(line), "\",\"marker\":\"");
    kbo_custom_news_json_append_escaped(line, sizeof(line), key);
    kbo_custom_news_json_append_raw(line, sizeof(line), "\",\"status\":\"");
    kbo_custom_news_json_append_escaped(line, sizeof(line), status);
    kbo_custom_news_json_append_raw(line, sizeof(line), "\",\"result\":");

    char result_text[32] = {0};
    snprintf(result_text, sizeof(result_text), "%d", result);
    kbo_custom_news_json_append_raw(line, sizeof(line), result_text);

    kbo_custom_news_json_append_raw(line, sizeof(line), ",\"title\":\"");
    kbo_custom_news_json_append_escaped(line, sizeof(line), title);
    kbo_custom_news_json_append_raw(line, sizeof(line), "\",\"detail\":\"");
    kbo_custom_news_json_append_escaped(line, sizeof(line), detail);
    kbo_custom_news_json_append_raw(line, sizeof(line), "\",\"source\":\"");
    kbo_custom_news_json_append_escaped(line, sizeof(line), source);
    kbo_custom_news_json_append_raw(line, sizeof(line), "\"}\r\n");

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(line);
    if (len == 0u || !WriteFile(file, line, len, &written, NULL) || written != len) {
        append_logf(
            "KBO custom news ledger write failed source=%s domain=%s key=%s gle=%lu path=%s",
            source != NULL ? source : "",
            domain != NULL ? domain : "",
            key != NULL ? key : "",
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}

void kbo_custom_news_ledger_record_completed(
    const char* domain,
    const char* key,
    const char* detail,
    const char* source)
{
    kbo_custom_news_ledger_record(
        domain,
        key,
        "completed",
        1,
        NULL,
        detail,
        source);
}
