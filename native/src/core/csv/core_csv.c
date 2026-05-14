#include "core_csv.h"

#include <stdlib.h>
#include <string.h>

void kbo_csv_trim_token_in_place(char* text)
{
    if (text == NULL) {
        return;
    }
    char* start = text;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1u);
    }
    size_t len = strlen(text);
    while (len > 0u
            && (text[len - 1u] == ' '
                || text[len - 1u] == '\t'
                || text[len - 1u] == '\r'
                || text[len - 1u] == '\n')) {
        text[--len] = '\0';
    }
}

uint32_t kbo_csv_parse_u32_text(const char* text, int base)
{
    if (text == NULL) {
        return 0u;
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    if (*text == '\0') {
        return 0u;
    }
    char* tail = NULL;
    unsigned long value = strtoul(text, &tail, base);
    if (tail == text || value > 0xfffffffful) {
        return 0u;
    }
    return (uint32_t)value;
}

int kbo_csv_parse_field(char** cursor, char* out, size_t out_size)
{
    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }

    size_t used = 0u;
    if (*p == '"') {
        p++;
        while (*p != '\0') {
            if (*p == '"') {
                if (p[1] == '"') {
                    if (used + 1u < out_size) {
                        out[used++] = '"';
                    }
                    p += 2;
                    continue;
                }
                p++;
                break;
            }
            if (used + 1u < out_size) {
                out[used++] = *p;
            }
            p++;
        }
        while (*p != '\0' && *p != ',' && *p != '\n') {
            p++;
        }
    } else {
        while (*p != '\0' && *p != ',' && *p != '\n') {
            if (used + 1u < out_size) {
                out[used++] = *p;
            }
            p++;
        }
        while (used > 0u && (out[used - 1u] == ' ' || out[used - 1u] == '\t' || out[used - 1u] == '\r')) {
            used--;
        }
    }

    out[used] = '\0';
    if (*p == ',') {
        p++;
    }
    *cursor = p;
    return 1;
}

int kbo_csv_parse_const_field(const char** cursor, char* out, size_t out_size)
{
    if (cursor == NULL || *cursor == NULL) {
        return 0;
    }
    char* mutable_cursor = (char*)*cursor;
    int ok = kbo_csv_parse_field(&mutable_cursor, out, out_size);
    *cursor = mutable_cursor;
    return ok;
}

int kbo_csv_parse_u32_field(const char** cursor, uint32_t* out_value)
{
    if (cursor == NULL || *cursor == NULL || out_value == NULL) {
        return 0;
    }

    const char* p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == ',') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    unsigned long long value = 0ull;
    while (*p >= '0' && *p <= '9') {
        value = (value * 10ull) + (unsigned long long)(*p - '0');
        if (value > UINT32_MAX) {
            return 0;
        }
        p++;
    }

    *out_value = (uint32_t)value;
    *cursor = p;
    return 1;
}
