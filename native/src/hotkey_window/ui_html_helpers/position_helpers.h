#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_HTML_HELPERS_POSITION_HELPERS_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_HTML_HELPERS_POSITION_HELPERS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

const char* kbo_webview_position_label_from_values(uint8_t position, uint8_t role);
const char* kbo_webview_player_position_label(uint8_t* player, uint8_t fallback_position);

#endif
