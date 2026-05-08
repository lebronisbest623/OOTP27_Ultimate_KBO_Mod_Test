#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_DRAFT_QUEUE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_DRAFT_QUEUE_H_

#include <stdint.h>
#include <windows.h>

typedef struct KboMilitaryDraftCandidate {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint16_t entry_year;
    uint8_t selected;
    uintptr_t player_ptr;
} KboMilitaryDraftCandidate;

extern KboMilitaryDraftCandidate g_kbo_military_draft_candidates[];
extern LONG g_kbo_military_draft_candidate_count;

int kbo_find_military_draft_candidate_index(uint32_t player_id, uint16_t entry_year);
int kbo_queue_military_draft_candidate(
    uintptr_t player_ptr,
    uint32_t player_id,
    uint16_t entry_year,
    uint32_t original_team_id,
    uint32_t original_league_id,
    const char* source);
int kbo_count_military_draft_candidates_for_year(uint16_t entry_year);

#endif
