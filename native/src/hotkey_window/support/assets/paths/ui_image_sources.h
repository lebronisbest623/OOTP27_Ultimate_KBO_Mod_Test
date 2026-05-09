#ifndef KBO_HOTKEY_WINDOW_UI_IMAGE_SOURCES_H
#define KBO_HOTKEY_WINDOW_UI_IMAGE_SOURCES_H

#include <stddef.h>

#include "../../text/buffer/ui_text_buffer.h"

void kbo_webview_copy_file_url(const char* path, char* out, size_t out_size);
void kbo_webview_append_image_src(KboWindowTextBuffer* buffer, const char* path);
void kbo_webview_copy_image_src(const char* path, char* out, size_t out_size);
void kbo_webview_append_dropdown_logo(KboWindowTextBuffer* buffer, const char* path);

#endif
