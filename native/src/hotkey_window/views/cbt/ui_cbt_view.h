#ifndef KBO_HOTKEY_WINDOW_UI_CBT_VIEW_H
#define KBO_HOTKEY_WINDOW_UI_CBT_VIEW_H

#include <stdint.h>

#include "../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_CBT_SUBVIEW_OVERVIEW 0
#define KBO_HUB_CBT_SUBVIEW_HISTORY  1
#define KBO_HUB_CBT_SUBVIEW_EXCEPTIONS 2
#define KBO_HUB_CBT_SUBVIEW_RULES    3
#define KBO_HUB_CBT_SUBVIEW_COUNT    4

void kbo_webview_append_cbt_view(KboWindowTextBuffer* buffer, int subview, uint32_t selected_team_id);

#endif
