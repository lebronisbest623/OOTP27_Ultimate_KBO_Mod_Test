#include "../internal/cbt_internal.h"

#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/news/ledger/core_news_ledger.h"

#define KBO_CBT_NEWS_LEDGER_DOMAIN "cbt"

static int kbo_cbt_news_marker_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("cbt_news_markers.txt", out, out_size);
}

int kbo_cbt_news_marker_exists(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 0;
    }
    if (kbo_custom_news_ledger_completed(KBO_CBT_NEWS_LEDGER_DOMAIN, key)) {
        return 1;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_news_marker_path(path, sizeof(path))) {
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
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 1024u * 1024u) {
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
    if (ReadFile(file, buffer, size, &read, NULL) && read == size) {
        buffer[size] = '\0';
        const size_t key_len = strlen(key);
        const char* cursor = buffer;
        while (*cursor != '\0') {
            while (*cursor == '\r' || *cursor == '\n') {
                cursor++;
            }
            const char* line = cursor;
            while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n') {
                cursor++;
            }
            if ((size_t)(cursor - line) == key_len && memcmp(line, key, key_len) == 0) {
                exists = 1;
                break;
            }
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    if (exists) {
        kbo_custom_news_ledger_record_completed(
            KBO_CBT_NEWS_LEDGER_DOMAIN,
            key,
            "legacy_marker_backfill",
            "cbt_news_marker_exists");
    }
    return exists;
}

void kbo_cbt_news_persist_marker(const char* key, const char* source)
{
    if (key == NULL || key[0] == '\0' || kbo_cbt_news_marker_exists(key)) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_news_marker_path(path, sizeof(path))) {
        append_logf(
            "KBO CBT news marker skipped source=%s key=%s reason=path_unavailable",
            source != NULL ? source : "",
            key);
        return;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf(
            "KBO CBT news marker skipped source=%s key=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            key,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    char line[160] = {0};
    int len = snprintf(line, sizeof(line), "%s\r\n", key);
    if (len > 0) {
        DWORD written = 0u;
        if (WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len) {
            kbo_custom_news_ledger_record_completed(
                KBO_CBT_NEWS_LEDGER_DOMAIN,
                key,
                "legacy_marker_persist",
                source);
        }
    }
    CloseHandle(file);
}
