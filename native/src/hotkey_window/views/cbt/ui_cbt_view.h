#ifndef KBO_HOTKEY_WINDOW_UI_CBT_VIEW_H
#define KBO_HOTKEY_WINDOW_UI_CBT_VIEW_H

#include "../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_CBT_SUBVIEW_RECORDS 0
#define KBO_HUB_CBT_SUBVIEW_RULES   1
#define KBO_HUB_CBT_SUBVIEW_COUNT   2

void kbo_webview_append_cbt_view(KboWindowTextBuffer* buffer, int subview);

#endif
