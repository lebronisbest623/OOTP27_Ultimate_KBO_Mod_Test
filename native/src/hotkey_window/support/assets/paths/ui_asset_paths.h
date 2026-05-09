#ifndef KBO_HOTKEY_WINDOW_UI_ASSET_PATHS_H
#define KBO_HOTKEY_WINDOW_UI_ASSET_PATHS_H

#include <stddef.h>

void kbo_hub_ootp_install_path(const char* relative_path, char* out, size_t out_size);
void kbo_hub_skin_image_path(const char* file_name, char* out, size_t out_size);
void kbo_hub_skin_scrollbar_image_path(const char* file_name, char* out, size_t out_size);
void kbo_hub_skin_button_image_path(const char* file_name, char* out, size_t out_size);
void kbo_hub_nation_flag_asset_path(const char* file_name, char* out, size_t out_size);
void kbo_hub_local_asset_path(const char* file_name, char* out, size_t out_size);
void kbo_hub_font_asset_path(const char* file_name, char* out, size_t out_size);
void kbo_hub_logo_asset_path(const char* file_name, char* out, size_t out_size);

#endif
