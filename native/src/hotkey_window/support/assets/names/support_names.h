#ifndef KBOFIX_SRC_HOTKEY_WINDOW_SUPPORT_NAMES_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_SUPPORT_NAMES_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>

void kbo_hub_draw_text(HDC hdc, const char* text, RECT rect, COLORREF color, HFONT font, UINT format);
int kbo_hub_ascii_is_alnum(char ch);
void kbo_hub_copy_player_display_name(uint8_t* player, char* out, size_t out_size);
void kbo_hub_copy_team_display_name_from_ptr(uint8_t* team, char* out, size_t out_size, const char* fallback);
void kbo_hub_copy_team_display_name_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback);
void kbo_hub_copy_team_abbrev_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback);

#endif