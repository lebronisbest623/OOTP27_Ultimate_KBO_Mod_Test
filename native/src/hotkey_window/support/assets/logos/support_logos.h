#ifndef KBOFIX_SRC_HOTKEY_WINDOW_SUPPORT_LOGOS_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_SUPPORT_LOGOS_H_

#include <stddef.h>
#include <stdint.h>

int kbo_hub_get_league_logo_path(uint32_t league_id, uint32_t year, char* out, size_t out_size);
int kbo_hub_get_team_logo_path(uint32_t team_id, uint32_t year, char* out, size_t out_size);

#endif