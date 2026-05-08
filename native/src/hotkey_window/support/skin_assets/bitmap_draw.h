#ifndef KBOFIX_SRC_HOTKEY_WINDOW_SUPPORT_SKIN_ASSETS_BITMAP_DRAW_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_SUPPORT_SKIN_ASSETS_BITMAP_DRAW_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void kbo_hub_draw_bitmap_alpha(HDC hdc, HBITMAP bitmap, const RECT* rect);
void kbo_hub_draw_vertical_three_piece(HDC hdc, HBITMAP top, HBITMAP mid, HBITMAP bottom, const RECT* rect);

#endif
