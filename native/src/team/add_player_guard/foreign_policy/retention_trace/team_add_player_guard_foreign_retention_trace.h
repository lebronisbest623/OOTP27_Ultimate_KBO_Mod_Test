#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_FOREIGN_RETENTION_TRACE_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_FOREIGN_RETENTION_TRACE_H_

#include <stdint.h>

void kbo_team_add_log_foreign_retention_result(
    uint32_t caller_rva,
    uint8_t result,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id);

#endif
