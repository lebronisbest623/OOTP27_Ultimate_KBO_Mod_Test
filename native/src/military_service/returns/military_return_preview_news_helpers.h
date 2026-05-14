#ifndef KBOFIX_SRC_MILITARY_SERVICE_RETURNS_MILITARY_RETURN_PREVIEW_NEWS_HELPERS_H_
#define KBOFIX_SRC_MILITARY_SERVICE_RETURNS_MILITARY_RETURN_PREVIEW_NEWS_HELPERS_H_

#include <stdint.h>
#include <stddef.h>

#define KBO_MILITARY_RETURN_PREVIEW_MAX 32
#define KBO_MILITARY_RETURN_PREVIEW_LIST_MAX 10

typedef struct KboMilitaryReturnPreviewEntry {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t return_yyyymmdd;
    int32_t days_left;
    int32_t score;
    uintptr_t player_ptr;
} KboMilitaryReturnPreviewEntry;

int kbo_military_return_preview_marker_exists(const char* key);
void kbo_military_return_preview_persist_marker(const char* key, const char* source);
int kbo_military_return_preview_collect(
    uint32_t today_serial,
    uint32_t return_year,
    KboMilitaryReturnPreviewEntry* entries,
    int max_entries);
void kbo_military_return_preview_player_name(
    const KboMilitaryReturnPreviewEntry* entry,
    char* out,
    size_t out_size);
int kbo_military_return_preview_render_body(
    const KboMilitaryReturnPreviewEntry* entries,
    int entry_count,
    char* out,
    size_t out_size,
    const char* source);

#endif
