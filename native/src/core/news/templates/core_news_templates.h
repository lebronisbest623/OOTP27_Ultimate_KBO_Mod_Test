#ifndef KBO_CORE_NEWS_TEMPLATES_H
#define KBO_CORE_NEWS_TEMPLATES_H

#include <stddef.h>

#define KBO_NEWS_TEMPLATES_DIR "news_templates"

typedef struct KboNewsTemplateVar {
    const char* key;
    const char* value;
} KboNewsTemplateVar;

int kbo_news_template_load(
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source);

int kbo_news_template_render(
    const char* tmpl,
    const KboNewsTemplateVar* vars,
    int var_count,
    char* out,
    size_t out_size);

int kbo_news_template_render_key(
    const char* key,
    const KboNewsTemplateVar* vars,
    int var_count,
    char* out,
    size_t out_size,
    const char* source);

void kbo_news_text_append(char* out, size_t out_size, const char* text);
void kbo_news_text_appendf(char* out, size_t out_size, const char* fmt, ...);
int kbo_news_text_ends_with_newline(const char* text);

#endif
