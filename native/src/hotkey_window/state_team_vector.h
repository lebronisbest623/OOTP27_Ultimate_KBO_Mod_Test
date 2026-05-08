#ifndef KBOFIX_SRC_HOTKEY_WINDOW_STATE_TEAM_VECTOR_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_STATE_TEAM_VECTOR_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_hub_get_team_vector(uintptr_t* out_vector, int32_t* out_count);

#endif
