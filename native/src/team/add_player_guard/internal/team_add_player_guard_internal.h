#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_INTERNAL_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_INTERNAL_H_

#include <windows.h>
#include <stdint.h>

void kbo_team_add_player_record_fa_compensation_success(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id);
int kbo_team_add_foreign_policy_should_block(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t team_id,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id);
void kbo_log_foreign_team_add_trace(
    uint32_t caller_rva,
    const char* result_label,
    uint8_t result,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id);
uint32_t kbo_team_add_cached_amateur_league_id(uint8_t* team);
int kbo_team_add_amateur_verbose_log_enabled_cached(void);
int kbo_team_add_retry_rejected_targets_enabled_cached(void);

#endif
