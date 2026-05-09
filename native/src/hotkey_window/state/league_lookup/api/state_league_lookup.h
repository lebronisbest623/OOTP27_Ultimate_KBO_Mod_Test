#ifndef KBOFIX_SRC_HOTKEY_WINDOW_STATE_LEAGUE_LOOKUP_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_STATE_LEAGUE_LOOKUP_H_

#include <stddef.h>
#include <stdint.h>

int kbo_hub_try_copy_cached_league_logo_file(uint32_t league_id, char* out, size_t out_size);
void kbo_hub_prewarm_league_display_cache(void);
void kbo_hub_copy_league_display_name_fast(uint32_t league_id, char* out, size_t out_size);
void kbo_hub_copy_league_display_name(uint32_t league_id, char* out, size_t out_size);

#endif