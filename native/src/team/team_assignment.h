#ifndef KBOFIX_SRC_TEAM_TEAM_ASSIGNMENT_H_
#define KBOFIX_SRC_TEAM_TEAM_ASSIGNMENT_H_

#include <stdint.h>

void kbo_assign_player_to_team_internal(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int update_organization,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach);

void kbo_assign_player_to_team_like_ootp(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach);

#endif
