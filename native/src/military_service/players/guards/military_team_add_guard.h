#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_TEAM_ADD_GUARD_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_TEAM_ADD_GUARD_H_

#include <stdint.h>

int kbo_military_team_add_player_should_block(uintptr_t team_ptr, uintptr_t player_ptr);
int kbo_military_player_has_active_service_assignment(uintptr_t player_ptr, uint32_t service_team_id);

#endif
