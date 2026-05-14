#ifndef KBO_CAPTAIN_NEWS_RENDER_H
#define KBO_CAPTAIN_NEWS_RENDER_H

#include <stdint.h>
#include <stddef.h>

struct KboCaptainSelectionRow;

int kbo_captain_news_render_key(
    const char* key,
    uint32_t season,
    const struct KboCaptainSelectionRow* row,
    const struct KboCaptainSelectionRow* old_row,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source);

#endif
