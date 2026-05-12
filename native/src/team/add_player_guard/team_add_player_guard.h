#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_H_

#include <stdint.h>

void kbo_set_team_add_player_guard_trampoline(void* trampoline);
void kbo_clear_team_add_player_guard_trampoline(void);
uint8_t kbo_team_add_player_guard_call_original(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8);

#endif
