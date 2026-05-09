#include "ui_text_buffer.h"

#include <stdarg.h>
#include <stdio.h>

void kbo_window_text_appendf(KboWindowTextBuffer* buffer, const char* format, ...)
{
    if (buffer == NULL || buffer->data == NULL || buffer->capacity == 0
            || buffer->length >= buffer->capacity - 1 || format == NULL) {
        return;
    }

    va_list args;
    va_start(args, format);
    int wrote = vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length, format, args);
    va_end(args);

    if (wrote <= 0) {
        return;
    }

    size_t available = buffer->capacity - buffer->length;
    if ((size_t)wrote >= available) {
        buffer->length = buffer->capacity - 1;
        buffer->data[buffer->length] = '\0';
        return;
    }

    buffer->length += (size_t)wrote;
}

void kbo_html_append_escaped(KboWindowTextBuffer* buffer, const char* text)
{
    if (buffer == NULL || text == NULL) {
        return;
    }
    for (const char* p = text; *p != '\0'; p++) {
        switch (*p) {
        case '&': kbo_window_text_appendf(buffer, "&amp;"); break;
        case '<': kbo_window_text_appendf(buffer, "&lt;"); break;
        case '>': kbo_window_text_appendf(buffer, "&gt;"); break;
        case '"': kbo_window_text_appendf(buffer, "&quot;"); break;
        case '\'': kbo_window_text_appendf(buffer, "&#39;"); break;
        default: kbo_window_text_appendf(buffer, "%c", *p); break;
        }
    }
}
