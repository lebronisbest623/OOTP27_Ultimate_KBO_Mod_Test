#ifndef KBOFIX_SRC_TEAM_TEAM_ORG_ASSIGNMENT_QUERY_H_
#define KBOFIX_SRC_TEAM_TEAM_ORG_ASSIGNMENT_QUERY_H_

#include <stdint.h>

int kbo_player_current_assignment_matches_team_or_affiliate(uint8_t* player, uint32_t team_id);

#endif
