#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_PLAYER_STATE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_PLAYER_STATE_H_

#include <stdint.h>

int32_t kbo_military_days_left(uint8_t* player);
int32_t kbo_military_effective_days_left(uint8_t* player);
uint32_t kbo_military_effective_return_yyyymmdd(uint8_t* player);
void kbo_set_military_days_left(uint8_t* player, int32_t days_left);
void kbo_clear_military_unavailable_flags(uint8_t* player);
void kbo_clear_military_status_flags(uint8_t* player);
void complete_kbo_military_service_status(uint8_t* player);
uint8_t* kbo_military_find_player_by_id(uint32_t player_id);
int kbo_military_resolve_original_team(
    uint8_t* player,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id,
    uint32_t* out_original_team_id,
    uint32_t* out_original_league_id);
void kbo_military_repair_original_team_memory(
    uint8_t* player,
    uint32_t original_team_id,
    uint32_t original_league_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id);

#endif
