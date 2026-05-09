#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_ROSTER_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_ROSTER_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_asian_games_roster_contains_player(uint32_t player_id);
int kbo_asian_games_roster_wildcard_count_except(LONG except_index);
int kbo_asian_games_replacement_allowed_for_org(uint32_t new_org_id, uint32_t old_org_id, LONG old_index);
int kbo_asian_games_player_unavailable_for_departure(uint8_t* player);

#endif
