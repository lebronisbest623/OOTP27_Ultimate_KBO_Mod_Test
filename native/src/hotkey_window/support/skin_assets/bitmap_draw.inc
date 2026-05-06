static void kbo_hub_draw_bitmap_alpha(HDC hdc, HBITMAP bitmap, const RECT* rect)
{
    if (hdc == NULL || bitmap == NULL || rect == NULL) {
        return;
    }

    BITMAP info;
    memset(&info, 0, sizeof(info));
    if (GetObject(bitmap, sizeof(info), &info) == 0 || info.bmWidth <= 0 || info.bmHeight <= 0) {
        return;
    }

    HDC memdc = CreateCompatibleDC(hdc);
    if (memdc == NULL) {
        return;
    }

    HGDIOBJ old = SelectObject(memdc, bitmap);
    BLENDFUNCTION blend;
    memset(&blend, 0, sizeof(blend));
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    AlphaBlend(
        hdc,
        rect->left, rect->top,
        rect->right - rect->left, rect->bottom - rect->top,
        memdc, 0, 0, info.bmWidth, info.bmHeight,
        blend);

    SelectObject(memdc, old);
    DeleteDC(memdc);
}

static void kbo_hub_draw_vertical_three_piece(HDC hdc, HBITMAP top, HBITMAP mid, HBITMAP bottom, const RECT* rect)
{
    if (hdc == NULL || rect == NULL || rect->right <= rect->left || rect->bottom <= rect->top) {
        return;
    }

    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    int cap = width;
    if (cap < 2) { cap = 2; }
    if (height < cap * 2) { cap = height / 2; }

    RECT top_rect    = {rect->left, rect->top,           rect->right, rect->top + cap};
    RECT bottom_rect = {rect->left, rect->bottom - cap,  rect->right, rect->bottom};
    RECT mid_rect    = {rect->left, top_rect.bottom,     rect->right, bottom_rect.top};

    if (top    != NULL) { kbo_hub_draw_bitmap_alpha(hdc, top,    &top_rect);    }
    if (mid    != NULL && mid_rect.bottom > mid_rect.top) { kbo_hub_draw_bitmap_alpha(hdc, mid, &mid_rect); }
    if (bottom != NULL) { kbo_hub_draw_bitmap_alpha(hdc, bottom, &bottom_rect); }
}
