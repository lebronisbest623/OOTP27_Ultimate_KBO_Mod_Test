#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_PLAYER_HOVER_MANAGER_PROBE_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_PLAYER_HOVER_MANAGER_PROBE_H_

#include <stdint.h>
#include <stddef.h>

uintptr_t kbo_player_hover_manager_ptr(void);
void kbo_set_player_tooltip_text_append_original(uintptr_t original_func_ptr);
void kbo_set_player_tooltip_string_format_original(uintptr_t original_func_ptr);
void kbo_set_player_tooltip_rating_common_original(uintptr_t original_func_ptr);
void kbo_set_player_tooltip_rating_panel_ctor_original(uintptr_t original_func_ptr);
int kbo_capture_ootp_player_tooltip_payload(uint32_t player_id, char* out, size_t out_size);
int kbo_show_ootp_player_hover_popup(uint32_t player_id, int screen_x, int screen_y);
void kbo_clear_ootp_player_hover_popup(uint32_t player_id);

#endif
