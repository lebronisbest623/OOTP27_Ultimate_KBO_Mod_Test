#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_FOREIGN_PURCHASE_RESTORE_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_FOREIGN_PURCHASE_RESTORE_H_

#include <stdint.h>

int kbo_team_add_restore_source_team_after_blocked_foreign_purchase(
    uint8_t* player,
    uint32_t source_team_id,
    uint32_t blocked_team_id,
    uint32_t caller_rva);

#endif
