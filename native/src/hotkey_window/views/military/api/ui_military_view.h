#ifndef KBO_HOTKEY_WINDOW_UI_MILITARY_VIEW_H
#define KBO_HOTKEY_WINDOW_UI_MILITARY_VIEW_H

#include <stdint.h>

#include "../../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_MILITARY_SUBVIEW_ROSTER     0
#define KBO_HUB_MILITARY_SUBVIEW_APPLICANTS 1
#define KBO_HUB_MILITARY_SUBVIEW_RESULTS    2
#define KBO_HUB_MILITARY_SUBVIEW_COUNT      3

void kbo_webview_append_military_view(
    KboWindowTextBuffer* buffer,
    int selected_military_subview,
    uint32_t* selected_results_year);

#endif
