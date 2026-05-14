#include "../core_news_templates.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void kbo_news_text_append(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0u || text == NULL) {
        return;
    }
    size_t used = strlen(out);
    if (used >= out_size - 1u) {
        return;
    }
    size_t remaining = out_size - used - 1u;
    size_t len = strlen(text);
    if (len > remaining) {
        len = remaining;
    }
    memcpy(out + used, text, len);
    out[used + len] = '\0';
}

void kbo_news_text_appendf(char* out, size_t out_size, const char* fmt, ...)
{
    if (out == NULL || out_size == 0u || fmt == NULL) {
        return;
    }
    size_t used = strlen(out);
    if (used >= out_size - 1u) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(out + used, out_size - used, fmt, args);
    va_end(args);
    out[out_size - 1u] = '\0';
}

static void kbo_news_text_append_char(char* out, size_t out_size, char ch)
{
    char text[2] = { ch, '\0' };
    kbo_news_text_append(out, out_size, text);
}

int kbo_news_text_ends_with_newline(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    size_t len = strlen(text);
    return len > 0u && (text[len - 1u] == '\n' || text[len - 1u] == '\r');
}

static int kbo_news_template_token_equals(const char* token, size_t token_len, const char* expected)
{
    return token != NULL
        && expected != NULL
        && strlen(expected) == token_len
        && memcmp(token, expected, token_len) == 0;
}

static const char* kbo_news_template_find_var(
    const KboNewsTemplateVar* vars,
    int var_count,
    const char* token,
    size_t token_len)
{
    if (vars == NULL || var_count <= 0 || token == NULL) {
        return NULL;
    }
    for (int i = 0; i < var_count; i++) {
        if (vars[i].key != NULL && kbo_news_template_token_equals(token, token_len, vars[i].key)) {
            return vars[i].value != NULL ? vars[i].value : "";
        }
    }
    return NULL;
}

int kbo_news_template_render(
    const char* tmpl,
    const KboNewsTemplateVar* vars,
    int var_count,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (tmpl == NULL) {
        return 0;
    }

    for (const char* p = tmpl; *p != '\0'; p++) {
        if (*p != '{') {
            kbo_news_text_append_char(out, out_size, *p);
            continue;
        }

        const char* close = strchr(p + 1, '}');
        if (close == NULL) {
            kbo_news_text_append_char(out, out_size, *p);
            continue;
        }

        const char* value = kbo_news_template_find_var(vars, var_count, p + 1, (size_t)(close - (p + 1)));
        if (value != NULL) {
            kbo_news_text_append(out, out_size, value);
        } else {
            kbo_news_text_append_char(out, out_size, '{');
            for (const char* q = p + 1; q < close; q++) {
                kbo_news_text_append_char(out, out_size, *q);
            }
            kbo_news_text_append_char(out, out_size, '}');
        }
        p = close;
    }
    return 1;
}
