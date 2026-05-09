#ifndef KBOFIX_SRC_HOTKEY_WINDOW_STATE_TEXT_UTILS_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_STATE_TEXT_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_utf8_to_wide(const char* text, WCHAR* out, int out_count);
void kbo_hub_trim_ascii(char* text);

#endif
