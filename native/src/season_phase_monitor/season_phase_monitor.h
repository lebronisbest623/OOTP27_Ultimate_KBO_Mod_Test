#ifndef KBOFIX_SRC_SEASON_PHASE_MONITOR_SEASON_PHASE_MONITOR_H_
#define KBOFIX_SRC_SEASON_PHASE_MONITOR_SEASON_PHASE_MONITOR_H_

#include <stdint.h>

__declspec(noinline) void ootp_kbo_season_phase_write_probe(
    uintptr_t league_ptr,
    uint32_t value,
    uint32_t site_rva);
int start_kbo_season_phase_monitor(void);

#endif
