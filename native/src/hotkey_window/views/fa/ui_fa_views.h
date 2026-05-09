#ifndef KBO_HOTKEY_WINDOW_UI_FA_VIEWS_H
#define KBO_HOTKEY_WINDOW_UI_FA_VIEWS_H

#include <stdint.h>

#include "../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_FA_SUBVIEW_MARKET          0
#define KBO_HUB_FA_SUBVIEW_COMPENSATION    1
#define KBO_HUB_FA_SUBVIEW_COUNT           2

void kbo_webview_append_fa_view(
    KboWindowTextBuffer* buffer,
    int selected_fa_subview,
    uint32_t selected_compensation_player_id,
    uint32_t selected_league_id);

#endif
