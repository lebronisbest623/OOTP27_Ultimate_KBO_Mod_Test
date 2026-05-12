#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SELECTION_EVENT_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SELECTION_EVENT_H_

#include <stdint.h>
#include <stddef.h>
#include <windows.h>
#include "../draft/military_draft_queue.h"
#include "../news/military_selection_news.h"

int run_kbo_custom_military_event(
    uintptr_t event_ptr,
    const char* event_name,
    uint32_t event_year,
    uint32_t event_month,
    uint32_t event_day,
    const char* source);
int kbo_refresh_military_selection_candidates_from_memory(
    uint16_t entry_year,
    uint32_t sang_id,
    const char* source);
void kbo_military_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size);
int kbo_military_draft_candidate_score(uint8_t* player);
int kbo_route_military_candidate_to_sang(
    KboMilitaryDraftCandidate* candidate,
    uint8_t* player,
    uint8_t* sang,
    uint32_t sang_id,
    uint32_t sang_league_id,
    int score,
    KboMilitarySelectionNewsEntry* news_entries,
    int max_news_entries,
    int routed,
    uint16_t entry_year,
    const char* source);
int kbo_route_queued_military_draft_candidates_ortools(
    uint16_t entry_year,
    uint8_t* sang,
    uint32_t sang_id,
    uint32_t sang_league_id,
    int slots,
    KboMilitarySelectionNewsEntry* news_entries,
    int max_news_entries,
    int* out_considered,
    const char* source);

#endif
