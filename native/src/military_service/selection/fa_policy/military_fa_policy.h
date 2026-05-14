#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_FA_POLICY_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_FA_POLICY_H_

#include <stdint.h>

uint32_t kbo_military_policy_current_yyyymmdd(void);
void kbo_record_recent_military_fa_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today);

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
int kbo_military_submit_offer_screen_should_block(
    uintptr_t screen_ptr,
    uint32_t player_id,
    uint32_t today,
    const char* source);
int kbo_military_ai_fa_candidate_should_block(
    uint32_t player_id,
    uint32_t requester_team_id,
    int32_t insert_index);
uint32_t kbo_military_fa_context_find_team_id(
    uintptr_t action_context,
    uint32_t* out_offset,
    uintptr_t* out_team_ptr);
int kbo_military_player_action_should_block(
    uintptr_t action_context,
    int32_t action_id,
    uint8_t strict_check);

#endif
