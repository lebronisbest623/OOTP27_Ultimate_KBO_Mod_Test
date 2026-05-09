#ifndef KBO_HOTKEY_WINDOW_UI_ROSTER_CELLS_H
#define KBO_HOTKEY_WINDOW_UI_ROSTER_CELLS_H

#include <stdint.h>

#include "../../text/buffer/ui_text_buffer.h"

void kbo_webview_append_player_name_cell(KboWindowTextBuffer* buffer, const char* player_name, uint32_t player_id);
void kbo_webview_append_player_name_link_cell(
    KboWindowTextBuffer* buffer,
    const char* player_name,
    uint32_t player_id,
    const char* href_prefix);
void kbo_webview_append_roster_top_bar(KboWindowTextBuffer* buffer, const char* right_text);

#endif
