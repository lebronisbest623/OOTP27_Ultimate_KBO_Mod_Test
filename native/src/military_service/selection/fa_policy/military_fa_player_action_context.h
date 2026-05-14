#ifndef KBO_MILITARY_SERVICE_SELECTION_FA_POLICY_MILITARY_FA_PLAYER_ACTION_CONTEXT_H_
#define KBO_MILITARY_SERVICE_SELECTION_FA_POLICY_MILITARY_FA_PLAYER_ACTION_CONTEXT_H_

#include <stdint.h>
#include <windows.h>

uint8_t* kbo_military_player_action_context_find_player(
    uintptr_t action_context,
    uint32_t* out_offset,
    uint32_t* out_player_id);

uint32_t kbo_military_player_action_context_find_team_id(
    uintptr_t action_context,
    uint32_t* out_offset,
    uintptr_t* out_team_ptr);

uint32_t kbo_military_fa_context_find_team_id(
    uintptr_t action_context,
    uint32_t* out_offset,
    uintptr_t* out_team_ptr);

#endif
