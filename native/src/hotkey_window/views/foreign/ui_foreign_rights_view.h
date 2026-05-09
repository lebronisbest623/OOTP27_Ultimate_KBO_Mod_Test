#ifndef KBO_HOTKEY_WINDOW_UI_FOREIGN_RIGHTS_VIEW_H
#define KBO_HOTKEY_WINDOW_UI_FOREIGN_RIGHTS_VIEW_H

#include <stdint.h>

#include "../../support/text/buffer/ui_text_buffer.h"

void kbo_webview_append_foreign_rights_view(
    KboWindowTextBuffer* buffer,
    const char* window_status,
    uint32_t selected_team_id,
    uint32_t* selected_foreign_player_id);

#endif
