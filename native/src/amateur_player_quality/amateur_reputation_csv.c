#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "amateur_reputation_csv.h"

static void kbo_amateur_reputation_trim_cell(char* value)
{
    if (value == NULL) {
        return;
    }
    char* start = value;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '"') {
        start++;
    }
    if (start != value) {
        memmove(value, start, strlen(start) + 1u);
    }
    size_t len = strlen(value);
    while (len > 0u) {
        char c = value[len - 1u];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '"') {
            break;
        }
        value[--len] = '\0';
    }
}

void kbo_amateur_reputation_read_cell(const char** cursor, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (cursor == NULL || *cursor == NULL) {
        return;
    }

    const char* p = *cursor;
    size_t len = 0u;
    int quoted = 0;
    if (*p == '"') {
        quoted = 1;
        p++;
    }
    while (*p != '\0') {
        if (quoted) {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (len + 1u < out_size) {
                        out[len++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                if (*p == ',') {
                    p++;
                }
                break;
            }
        } else if (*p == ',') {
            p++;
            break;
        } else if (*p == '\r' || *p == '\n') {
            break;
        }
        if (len + 1u < out_size) {
            out[len++] = *p;
        }
        p++;
    }
    out[len] = '\0';
    kbo_amateur_reputation_trim_cell(out);
    *cursor = p;
}

uint32_t kbo_amateur_reputation_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    unsigned long value = strtoul(text, NULL, 10);
    if (value > UINT32_MAX) {
        return 0u;
    }
    return (uint32_t)value;
}

int kbo_read_amateur_reputation_seed_file(const char* path, char** out_buffer, DWORD* out_size)
{
    if (out_buffer == NULL || out_size == NULL) {
        return 0;
    }
    *out_buffer = NULL;
    *out_size = 0;
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    LARGE_INTEGER file_size = {0};
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }

    DWORD size = (DWORD)file_size.QuadPart;
    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int ok = ReadFile(file, buffer, size, &read, NULL) && read == size;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    buffer[size] = '\0';
    *out_buffer = buffer;
    *out_size = size;
    return 1;
}
