#ifndef KBO_HOTKEY_WINDOW_UI_NATION_HELPERS_H
#define KBO_HOTKEY_WINDOW_UI_NATION_HELPERS_H

#include <stddef.h>
#include <stdint.h>

#include "../../text/buffer/ui_text_buffer.h"

typedef void (*KboHubNationFlagAssetPathFn)(const char* file_name, char* out, size_t out_size);

void kbo_webview_append_roster_nation_cell(
    KboWindowTextBuffer* buffer,
    uint32_t nation_id,
    KboHubNationFlagAssetPathFn flag_asset_path);

#endif
