#ifndef KBO_CORE_NEWS_TEMPLATES_INTERNAL_H
#define KBO_CORE_NEWS_TEMPLATES_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>

int kbo_news_template_file_exists(const char* path);

char* kbo_news_template_read_json_file(const char* path, DWORD* out_size);

int kbo_news_template_load_from_path(
    const char* path,
    const char* key,
    char* out,
    size_t out_size);

int kbo_news_template_load_int_from_path(const char* path, const char* key, int* out_value);

int kbo_news_template_try_load_from_existing_path(
    const char* path,
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size);

int kbo_news_template_file_for_key(
    const char* key,
    char* out,
    size_t out_size,
    const char* source);

#endif
