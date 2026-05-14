#include "../internal/captain_selection_internal.h"

#include "captain_news_markers.h"

static int kbo_captain_news_marker_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("captain_news_markers.txt", out, out_size);
}

int kbo_captain_news_marker_exists(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_news_marker_path(path, sizeof(path))) {
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

    DWORD high = 0;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > (128u * 1024u)) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        size_t key_len = strlen(key);
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            size_t line_len = (size_t)(line_end - cursor);
            if (line_len == key_len && memcmp(cursor, key, key_len) == 0) {
                found = 1;
                break;
            }
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

void kbo_captain_news_persist_marker(const char* key, const char* source)
{
    if (key == NULL || key[0] == '\0' || kbo_captain_news_marker_exists(key)) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_news_marker_path(path, sizeof(path))) {
        append_logf(
            "KBO captain news marker skipped source=%s key=%s reason=path_unavailable",
            source != NULL ? source : "",
            key);
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
            "KBO captain news marker skipped source=%s key=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            key,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    char line[256] = {0};
    int len = snprintf(line, sizeof(line), "%s\r\n", key);
    DWORD written = 0;
    if (len <= 0 || len >= (int)sizeof(line)
            || !WriteFile(file, line, (DWORD)len, &written, NULL)
            || written != (DWORD)len) {
        append_logf(
            "KBO captain news marker write failed source=%s key=%s gle=%lu path=%s",
            source != NULL ? source : "",
            key,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}
