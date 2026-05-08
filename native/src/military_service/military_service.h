#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_H_

#include <stdint.h>
#include <windows.h>

#define KBO_MILITARY_SERVICE_DAYS 545

typedef struct KboMilitaryDraftCandidate {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint16_t entry_year;
    uint8_t selected;
    uintptr_t player_ptr;
} KboMilitaryDraftCandidate;

extern LONG g_active_military_loan_count;
extern KboMilitaryDraftCandidate g_kbo_military_draft_candidates[];
extern LONG g_kbo_military_draft_candidate_count;

void kbo_load_military_service_team_policy_override_once(void);
void start_kbo_military_seed_bootstrap_thread(void);
void start_kbo_military_days_tick_thread(void);

int kbo_military_fa_candidate_fast_block(
    uintptr_t player_ptr,
    uint32_t requester_team_id,
    const char* context,
    uint32_t* out_player_id);
int kbo_military_offer_eligibility_should_block(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t flag,
    uint8_t original_result,
    uint32_t* out_player_id);
int kbo_military_signability_should_block(
    uint32_t player_id,
    int32_t requesting_team_id,
    int original_signability,
    uintptr_t caller_rva);
int kbo_military_submit_offer_should_block(uintptr_t screen_ptr, uint32_t player_id, uint32_t today);
int kbo_military_ai_fa_candidate_should_block(
    uint32_t player_id,
    uint32_t requester_team_id,
    int32_t insert_index);
int kbo_military_player_action_should_block(
    uintptr_t action_context,
    int32_t action_id,
    uint8_t strict_check);

#endif
