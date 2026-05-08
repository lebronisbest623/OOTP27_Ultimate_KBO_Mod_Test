#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_FA_BLOCK_STATE_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_FA_BLOCK_STATE_H_

#include <stdint.h>
#include <windows.h>

extern volatile LONG g_kbo_foreign_offer_block_player_id;
extern volatile LONG g_kbo_foreign_offer_block_requester_team_id;
extern volatile LONG g_kbo_foreign_offer_block_holder_team_id;
extern volatile LONG g_kbo_custom_foreign_policy_block_player_id;
extern volatile LONG g_kbo_custom_foreign_policy_block_requester_team_id;

int kbo_recent_foreign_offer_allow_matches(
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_requester_team_id);
int kbo_recent_custom_foreign_policy_allow_matches(
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_requester_team_id);
int kbo_recent_foreign_offer_block_matches(
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_requester_team_id,
    uint32_t* out_holder_team_id);
int kbo_recent_custom_foreign_policy_block_matches(
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_requester_team_id);

#endif
