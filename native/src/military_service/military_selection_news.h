#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SELECTION_NEWS_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SELECTION_NEWS_H_

#include <stddef.h>
#include <stdint.h>

typedef struct KboMilitarySelectionNewsEntry {
    uint32_t player_id;
    uint32_t original_team_id;
    int32_t score;
    uintptr_t player_ptr;
} KboMilitarySelectionNewsEntry;

void kbo_military_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size);
void kbo_military_copy_team_history_name(
    uint8_t* team,
    char* out,
    size_t out_size,
    const char* fallback);
int kbo_emit_military_selection_news(
    uint32_t event_yyyymmdd,
    KboMilitarySelectionNewsEntry* entries,
    int entry_count,
    const char* source);

#endif
