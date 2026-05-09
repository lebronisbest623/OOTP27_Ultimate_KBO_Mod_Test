#include "ui_js_string.h"

void kbo_webview_append_js_string(KboWindowTextBuffer* buffer, const char* text)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "'");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            unsigned char ch = (unsigned char)*p;
            if (ch == '\\' || ch == '\'') {
                kbo_window_text_appendf(buffer, "\\%c", ch);
            } else if (ch == '\n') {
                kbo_window_text_appendf(buffer, "\\n");
            } else if (ch == '\r') {
                kbo_window_text_appendf(buffer, "\\r");
            } else if (ch < 0x20u) {
                kbo_window_text_appendf(buffer, "\\x%02x", (unsigned int)ch);
            } else {
                kbo_window_text_appendf(buffer, "%c", ch);
            }
        }
    }
    kbo_window_text_appendf(buffer, "'");
}
