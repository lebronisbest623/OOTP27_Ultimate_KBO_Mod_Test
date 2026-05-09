#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_SEASON_PHASE_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_SEASON_PHASE_H_

#include <stdint.h>

uint8_t* build_kbo_season_phase_opening_day_event_stub(
    void* patch_site,
    uint8_t base_reg,
    uint32_t site_rva,
    uint8_t value);

#endif
