#include "state_text_utils.h"

int kbo_utf8_to_wide(const char* text, WCHAR* out, int out_count)
{
    if (text == NULL || out == NULL || out_count <= 0) {
        return 0;
    }

    int wrote = MultiByteToWideChar(CP_UTF8, 0, text, -1, out, out_count);
    if (wrote <= 0) {
        wrote = MultiByteToWideChar(CP_ACP, 0, text, -1, out, out_count);
    }
    if (wrote <= 0) {
        out[0] = L'\0';
        return 0;
    }
    return 1;
}

void kbo_hub_trim_ascii(char* text)
{
    if (text == NULL) {
        return;
    }

    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\r' || text[len - 1] == '\n'
                       || text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[--len] = '\0';
    }

    size_t start = 0;
    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }
    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}
