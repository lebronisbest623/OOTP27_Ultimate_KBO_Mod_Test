#ifndef KBOFIX_SRC_HOOK_STUBS_FOREIGN_SIGNABILITY_STUBS_OFFER_CALLBACK_STUBS_H_
#define KBOFIX_SRC_HOOK_STUBS_FOREIGN_SIGNABILITY_STUBS_OFFER_CALLBACK_STUBS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_fa_offer_screen_callback_probe_detour_stub(void* original_trampoline);
uint8_t* build_kbo_fa_contract_offer_callback_probe_detour_stub(void* original_trampoline);
uint8_t* build_kbo_player_action_eligibility_detour_stub(void* original_trampoline);

#endif
