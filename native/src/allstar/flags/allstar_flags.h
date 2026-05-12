#ifndef KBOFIX_SRC_ALLSTAR_ALLSTAR_FLAGS_H_
#define KBOFIX_SRC_ALLSTAR_ALLSTAR_FLAGS_H_

#include <stdint.h>

void enable_kbo_allstar_flags(uintptr_t league_ptr, const char* source);
int enable_kbo_allstar_raw_flags_if_kbo_context(uintptr_t league_ptr, const char* source);
int force_kbo_allstar_flags_for_league_pointer(uintptr_t league_ptr, const char* source);
void force_kbo_allstar_flags_for_configured_league(const char* source);
void start_kbo_allstar_force_retry_thread(void);
uintptr_t find_kbo_allstar_core_fallback_league(uint32_t league_id);
int enable_kbo_allstar_flags_for_core_league(uintptr_t league_ptr, uint32_t league_id, const char* source);

#endif
