#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_ALLSTAR_SETTINGS_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_ALLSTAR_SETTINGS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_allstar_settings_enable_stub(void* return_address, void* checkbox_set_bool_address, uint32_t game_flag_offset);

#endif
