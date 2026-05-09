#ifndef KBOFIX_SRC_HOOK_STUBS_FOREIGN_SIGNABILITY_STUBS_BASIC_ELIGIBILITY_STUBS_H_
#define KBOFIX_SRC_HOOK_STUBS_FOREIGN_SIGNABILITY_STUBS_BASIC_ELIGIBILITY_STUBS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_player_team_signability_detour_stub(void* original_trampoline);
uint8_t* build_kbo_player_offer_eligibility_detour_stub(void* original_trampoline);
uint8_t* build_kbo_fa_submit_offer_probe_detour_stub(void* original_trampoline);

#endif
