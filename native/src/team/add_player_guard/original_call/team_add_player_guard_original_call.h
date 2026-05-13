#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_ORIGINAL_CALL_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_ORIGINAL_CALL_H_

#include <stdint.h>

typedef uint8_t (__fastcall *KboTeamAddPlayerOriginalFn)(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8);

KboTeamAddPlayerOriginalFn kbo_team_add_player_guard_get_original(void);
int kbo_team_add_original_args_readable(uintptr_t team_ptr, uintptr_t player_ptr);
void kbo_team_add_log_skipped_bad_original_args(
    uint32_t caller_rva,
    const char* stage,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t original_team_ptr);

#endif
