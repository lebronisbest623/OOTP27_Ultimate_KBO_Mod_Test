#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>
#include <string.h>

#include "ootp_text_encoding.h"

int kbo_text_has_non_ascii(const char* text)
{
    const unsigned char* p = (const unsigned char*)text;
    while (p != NULL && *p != '\0') {
        if (*p >= 0x80) {
            return 1;
        }
        ++p;
    }
    return 0;
}

static unsigned int kbo_decode_utf8_codepoint(const unsigned char** cursor)
{
    const unsigned char* p = cursor != NULL ? *cursor : NULL;
    if (p == NULL || *p == '\0') {
        return 0;
    }

    unsigned char c = *p;
    if (c < 0x80) {
        *cursor = p + 1;
        return c;
    }

    if ((c & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
        unsigned int cp = ((unsigned int)(c & 0x1f) << 6) | (unsigned int)(p[1] & 0x3f);
        if (cp >= 0x80) {
            *cursor = p + 2;
            return cp;
        }
    } else if ((c & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
        unsigned int cp = ((unsigned int)(c & 0x0f) << 12)
            | ((unsigned int)(p[1] & 0x3f) << 6)
            | (unsigned int)(p[2] & 0x3f);
        if (cp >= 0x800 && !(cp >= 0xd800 && cp <= 0xdfff)) {
            *cursor = p + 3;
            return cp;
        }
    } else if ((c & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
        unsigned int cp = ((unsigned int)(c & 0x07) << 18)
            | ((unsigned int)(p[1] & 0x3f) << 12)
            | ((unsigned int)(p[2] & 0x3f) << 6)
            | (unsigned int)(p[3] & 0x3f);
        if (cp >= 0x10000 && cp <= 0x10ffff) {
            *cursor = p + 4;
            return cp;
        }
    }

    *cursor = p + 1;
    return c;
}

static char kbo_ootp_hex_digit(unsigned int value)
{
    return (char)(value < 10u ? ('0' + value) : ('a' + (value - 10u)));
}

static void kbo_write_ootp_unicode_escape(char** cursor, unsigned int cp)
{
    char* out = cursor != NULL ? *cursor : NULL;
    if (out == NULL) {
        return;
    }

    *out++ = ',';
    *out++ = kbo_ootp_hex_digit((cp >> 12) & 0x0fu);
    *out++ = kbo_ootp_hex_digit((cp >> 8) & 0x0fu);
    *out++ = kbo_ootp_hex_digit((cp >> 4) & 0x0fu);
    *out++ = kbo_ootp_hex_digit(cp & 0x0fu);
    *cursor = out;
}

char* kbo_alloc_ootp_internal_text(const char* text)
{
    if (text == NULL || text[0] == '\0' || !kbo_text_has_non_ascii(text)) {
        return NULL;
    }

    size_t len = strlen(text);
    if (len == 0 || len > 100000u) {
        return NULL;
    }

    size_t capacity = 1u;
    int unicode_mode = 0;
    const unsigned char* scan = (const unsigned char*)text;
    while (*scan != '\0') {
        unsigned int cp = kbo_decode_utf8_codepoint(&scan);
        if (cp > 0xffffu) {
            if (unicode_mode) {
                ++capacity;
                unicode_mode = 0;
            }
            ++capacity;
        } else if (cp > 0xffu) {
            if (!unicode_mode) {
                ++capacity;
                unicode_mode = 1;
            }
            capacity += 5u;
        } else {
            if (unicode_mode) {
                ++capacity;
                unicode_mode = 0;
            }
            ++capacity;
        }
    }
    if (unicode_mode) {
        ++capacity;
    }

    if (capacity == 0 || capacity > 600000u) {
        return NULL;
    }

    char* out = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, capacity);
    if (out == NULL) {
        return NULL;
    }

    char* cursor = out;
    const unsigned char* in = (const unsigned char*)text;
    unicode_mode = 0;
    while (*in != '\0') {
        unsigned int cp = kbo_decode_utf8_codepoint(&in);
        if (cp > 0xffffu) {
            if (unicode_mode) {
                *cursor++ = '\x01';
                unicode_mode = 0;
            }
            *cursor++ = '?';
            continue;
        }

        if (cp > 0xffu) {
            if (!unicode_mode) {
                *cursor++ = '\x01';
                unicode_mode = 1;
            }
            kbo_write_ootp_unicode_escape(&cursor, cp);
        } else {
            if (unicode_mode) {
                *cursor++ = '\x01';
                unicode_mode = 0;
            }
            *cursor++ = (char)(cp & 0xffu);
        }
    }
    if (unicode_mode) {
        *cursor++ = '\x01';
    }
    *cursor = '\0';
    return out;
}

void kbo_free_ootp_internal_text(char* text)
{
    if (text != NULL) {
        HeapFree(GetProcessHeap(), 0, text);
    }
}
