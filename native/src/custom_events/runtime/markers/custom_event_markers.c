#include "../common/custom_events_common.h"
#include "custom_event_markers.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

int kbo_get_custom_event_processed_marker_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("custom_events_processed.txt", out, out_size);
}

int kbo_custom_event_processed_marker_exists(uint32_t event_yyyymmdd, const char* name)
{
    if (event_yyyymmdd == 0u || name == NULL || name[0] == '\0') {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_custom_event_processed_marker_path(path, sizeof(path))) {
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
    if (size == INVALID_FILE_SIZE || high != 0 || size == 0u || size > (256u * 1024u)) {
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

        char key[192] = {0};
        int key_len = snprintf(key, sizeof(key), "%u|%s", event_yyyymmdd, name);
        if (key_len > 0 && key_len < (int)sizeof(key)) {
            char* cursor = buffer;
            char* end = buffer + read;
            while (cursor < end) {
                char* line_end = cursor;
                while (line_end < end && *line_end != '\n' && *line_end != '\r') {
                    line_end++;
                }
                size_t line_len = (size_t)(line_end - cursor);
                if (line_len == (size_t)key_len && memcmp(cursor, key, line_len) == 0) {
                    found = 1;
                    break;
                }
                while (line_end < end && (*line_end == '\n' || *line_end == '\r')) {
                    line_end++;
                }
                cursor = line_end;
            }
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return found;
}

void kbo_persist_custom_event_processed_marker(uint32_t event_yyyymmdd, const char* name, const char* source)
{
    if (event_yyyymmdd == 0u || name == NULL || name[0] == '\0') {
        return;
    }
    if (kbo_custom_event_processed_marker_exists(event_yyyymmdd, name)) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_custom_event_processed_marker_path(path, sizeof(path))) {
        append_logf(
            "KBO custom event marker skipped source=%s name=%s date=%u reason=path_unavailable",
            source != NULL ? source : "",
            name,
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
        append_logf(
            "KBO custom event marker skipped source=%s name=%s date=%u reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            name,
            event_yyyymmdd,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    char line[256] = {0};
    int len = snprintf(line, sizeof(line), "%u|%s\r\n", event_yyyymmdd, name);
    DWORD written = 0;
    if (len <= 0 || len >= (int)sizeof(line)
            || !WriteFile(file, line, (DWORD)len, &written, NULL)
            || written != (DWORD)len) {
        append_logf(
            "KBO custom event marker write failed source=%s name=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            name,
            event_yyyymmdd,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}

void kbo_mark_custom_event_over(uintptr_t event_ptr)
{
    if (event_ptr == 0
            || !memory_range_readable((void*)event_ptr, OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET + sizeof(uint16_t))) {
        return;
    }

    uint16_t* event_over = (uint16_t*)((uint8_t*)event_ptr + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
    DWORD old_protect = 0;
    if (VirtualProtect(event_over, sizeof(*event_over), PAGE_READWRITE, &old_protect)) {
        *event_over = 1u;
        DWORD ignored = 0;
        VirtualProtect(event_over, sizeof(*event_over), old_protect, &ignored);
    }
}

int kbo_custom_event_already_processed(uintptr_t event_ptr)
{
    LONG count = g_kbo_processed_event_count;
    if (count < 0) {
        count = 0;
    }
    if (count > (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]))) {
        count = (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]));
    }

    for (LONG i = 0; i < count; i++) {
        if (g_kbo_processed_event_ptrs[i] == event_ptr) {
            return 1;
        }
    }
    return 0;
}

void kbo_mark_custom_event_processed(uintptr_t event_ptr)
{
    if (event_ptr == 0) {
        return;
    }
    kbo_mark_custom_event_over(event_ptr);
    if (kbo_custom_event_already_processed(event_ptr)) {
        return;
    }

    LONG slot = InterlockedIncrement(&g_kbo_processed_event_count) - 1;
    if (slot < 0 || slot >= (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]))) {
        InterlockedDecrement(&g_kbo_processed_event_count);
        return;
    }
    g_kbo_processed_event_ptrs[slot] = event_ptr;
}
