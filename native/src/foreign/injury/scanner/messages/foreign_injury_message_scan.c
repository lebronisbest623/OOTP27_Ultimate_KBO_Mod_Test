#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../foreign_injury_scanner_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../core/files/save_paths/core_save_paths.h"

static int kbo_foreign_injury_message_ascii_lower(int ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch;
}

static int kbo_foreign_injury_message_contains_nocase(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    for (const char* p = text; *p != '\0'; ++p) {
        const char* a = p;
        const char* b = needle;
        while (*a != '\0' && *b != '\0'
                && kbo_foreign_injury_message_ascii_lower((unsigned char)*a)
                    == kbo_foreign_injury_message_ascii_lower((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (*b == '\0') {
            return 1;
        }
    }
    return 0;
}

static int kbo_foreign_injury_message_file_has_long_term_injury(
    const char* path,
    uint32_t player_id,
    int min_days,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (path == NULL || path[0] == '\0' || player_id == 0u || min_days <= 0) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 65536u) {
        CloseHandle(file);
        return 0;
    }

    char* data = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (data == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    BOOL ok = ReadFile(file, data, size, &read, NULL);
    CloseHandle(file);
    if (!ok || read == 0u) {
        HeapFree(GetProcessHeap(), 0, data);
        return 0;
    }
    data[read < size ? read : size] = '\0';

    char player_tag[32] = {0};
    snprintf(player_tag, sizeof(player_tag), "player#%u", player_id);
    int evidence_days = 0;
    int found = strstr(data, player_tag) != NULL
        && (kbo_foreign_injury_message_contains_nocase(data, "injured list")
            || kbo_foreign_injury_message_contains_nocase(data, "injury")
            || kbo_foreign_injury_message_contains_nocase(data, "injured")
            || kbo_foreign_injury_message_contains_nocase(data, "sidelined")
            || kbo_foreign_injury_message_contains_nocase(data, "out of commission"))
        && kbo_foreign_injury_duration_text_meets_minimum(data, min_days, &evidence_days);
    HeapFree(GetProcessHeap(), 0, data);

    if (found && out_days != NULL) {
        *out_days = evidence_days;
    }
    return found;
}

int kbo_foreign_injury_recent_message_has_long_term_injury(
    uint32_t player_id,
    int min_days,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (player_id == 0u || min_days <= 0) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char pattern[1024] = {0};
    snprintf(pattern, sizeof(pattern), "%s\\messages\\message*.txt", save_path);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int found = 0;
    int best_days = 0;
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        char path[1024] = {0};
        snprintf(path, sizeof(path), "%s\\messages\\%s", save_path, find_data.cFileName);
        int evidence_days = 0;
        if (kbo_foreign_injury_message_file_has_long_term_injury(
                path,
                player_id,
                min_days,
                &evidence_days)) {
            found = 1;
            if (evidence_days > best_days) {
                best_days = evidence_days;
            }
        }
    } while (FindNextFileA(find, &find_data));
    FindClose(find);

    if (out_days != NULL) {
        *out_days = best_days;
    }
    return found;
}
