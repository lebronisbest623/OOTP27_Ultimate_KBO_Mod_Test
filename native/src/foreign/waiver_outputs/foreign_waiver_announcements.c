#include "foreign_waiver_announcements.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../common/paths/foreign_waiver_paths.h"

int kbo_foreign_waiver_announcement_recorded(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char raw[4096] = {0};
    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, raw, sizeof(raw) - 1u, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        const char* cursor = raw;
        while (*cursor != '\0') {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n' || *cursor == ',') {
                cursor++;
            }
            char* tail = NULL;
            unsigned long raw_date = strtoul(cursor, &tail, 10);
            if (raw_date == event_yyyymmdd) {
                found = 1;
                break;
            }
            if (tail == cursor) {
                break;
            }
            cursor = tail;
        }
    }

    CloseHandle(file);
    return found;
}

void kbo_record_foreign_waiver_announcement(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char line[32] = {0};
    int len = snprintf(line, sizeof(line), "%u\r\n", event_yyyymmdd);
    DWORD written = 0;
    if (len > 0) {
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);
}

void kbo_record_foreign_waiver_announcement_body(uint32_t event_yyyymmdd, const char* source, const char* body)
{
    if (event_yyyymmdd == 0u) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_announcement_path(path, sizeof(path))) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign reserve rights: announcement body record failed path=%s err=%lu", path, GetLastError());
        return;
    }

    char line[1400] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%u|%s|%s\r\n",
        event_yyyymmdd,
        source != NULL ? source : "",
        body != NULL ? body : "");
    DWORD written = 0;
    if (len > 0) {
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);
}
