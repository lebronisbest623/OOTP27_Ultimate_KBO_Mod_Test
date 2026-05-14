#ifndef KBOFIX_SRC_HOTKEY_WINDOW_VIEWS_FUTURES_UI_FUTURES_LEAGUE_VIEW_HELPERS_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_VIEWS_FUTURES_UI_FUTURES_LEAGUE_VIEW_HELPERS_H_

#include <stdint.h>
#include <stddef.h>

void kbo_futures_ui_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size);
void kbo_futures_ui_format_cash(int32_t value, char* out, size_t out_size);
void kbo_futures_ui_copy_player_name(uintptr_t player_ptr, uint32_t player_id, char* out, size_t out_size);
const char* kbo_futures_ui_position_label(uintptr_t player_ptr);
void kbo_futures_ui_copy_team_name(uint32_t team_id, char* out, size_t out_size);
uint32_t kbo_futures_ui_resolve_buyer_team_id(uint32_t selected_team_id);

#endif
