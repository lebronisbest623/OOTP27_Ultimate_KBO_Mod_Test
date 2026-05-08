#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_MILITARY_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_MILITARY_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_military_service_entry_trampoline(void* original_address, size_t stolen_len);
uint8_t* build_kbo_military_service_entry_detour_stub(void* original_trampoline);
uint8_t* build_kbo_military_status_update_detour_stub(void* original_trampoline);
uint8_t* build_kbo_team_add_player_guard_detour_stub(void);

#endif
