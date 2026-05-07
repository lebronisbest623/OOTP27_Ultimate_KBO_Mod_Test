static int kbo_hub_estimate_visible_edit_lines(void)
{
    if (g_kbo_hotkey_edit == NULL || g_kbo_hub_content_rect.bottom <= g_kbo_hub_content_rect.top) {
        return 1;
    }

    HDC hdc = GetDC(g_kbo_hotkey_edit);
    if (hdc == NULL) {
        return 1;
    }

    HFONT font = (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES)
        ? g_kbo_hub_font_mono : g_kbo_hub_font_body;
    HGDIOBJ old = NULL;
    if (font != NULL) {
        old = SelectObject(hdc, font);
    }

    TEXTMETRICA metrics;
    memset(&metrics, 0, sizeof(metrics));
    int line_height = 16;
    if (GetTextMetricsA(hdc, &metrics)) {
        line_height = metrics.tmHeight + metrics.tmExternalLeading;
        if (line_height < 8) { line_height = 8; }
    }

    if (old != NULL) { SelectObject(hdc, old); }
    ReleaseDC(g_kbo_hotkey_edit, hdc);

    int content_height = g_kbo_hub_content_rect.bottom - g_kbo_hub_content_rect.top;
    int visible = content_height / line_height;
    return visible > 0 ? visible : 1;
}

static void kbo_hub_update_scrollbar_geometry(void)
{
    SetRectEmpty(&g_kbo_hub_scrollbar_thumb_rect);
    if (g_kbo_hotkey_edit == NULL || g_kbo_hub_scrollbar_rect.bottom <= g_kbo_hub_scrollbar_rect.top) {
        return;
    }

    int button = g_kbo_hub_skin_scrollbar_width;
    if (button < 12) { button = 12; }

    g_kbo_hub_scrollbar_less_rect = g_kbo_hub_scrollbar_rect;
    g_kbo_hub_scrollbar_less_rect.bottom = g_kbo_hub_scrollbar_less_rect.top + button;
    g_kbo_hub_scrollbar_more_rect = g_kbo_hub_scrollbar_rect;
    g_kbo_hub_scrollbar_more_rect.top = g_kbo_hub_scrollbar_more_rect.bottom - button;

    int total_lines   = (int)SendMessageA(g_kbo_hotkey_edit, EM_GETLINECOUNT, 0, 0);
    int first_line    = (int)SendMessageA(g_kbo_hotkey_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
    int visible_lines = kbo_hub_estimate_visible_edit_lines();
    int track_top     = g_kbo_hub_scrollbar_less_rect.bottom;
    int track_bottom  = g_kbo_hub_scrollbar_more_rect.top;
    int track_height  = track_bottom - track_top;
    if (track_height <= 0 || total_lines <= visible_lines) {
        g_kbo_hub_scrollbar_thumb_rect.left   = g_kbo_hub_scrollbar_rect.left;
        g_kbo_hub_scrollbar_thumb_rect.right  = g_kbo_hub_scrollbar_rect.right;
        g_kbo_hub_scrollbar_thumb_rect.top    = track_top;
        g_kbo_hub_scrollbar_thumb_rect.bottom = track_bottom;
        return;
    }

    int thumb_h = (visible_lines * track_height) / total_lines;
    if (thumb_h < button)       { thumb_h = button; }
    if (thumb_h > track_height) { thumb_h = track_height; }

    int max_first = total_lines - visible_lines;
    if (max_first < 1) { max_first = 1; }
    if (first_line < 0) { first_line = 0; }
    if (first_line > max_first) { first_line = max_first; }
    int thumb_y = track_top + ((track_height - thumb_h) * first_line) / max_first;

    g_kbo_hub_scrollbar_thumb_rect.left   = g_kbo_hub_scrollbar_rect.left;
    g_kbo_hub_scrollbar_thumb_rect.right  = g_kbo_hub_scrollbar_rect.right;
    g_kbo_hub_scrollbar_thumb_rect.top    = thumb_y;
    g_kbo_hub_scrollbar_thumb_rect.bottom = thumb_y + thumb_h;
}

static void kbo_hub_draw_ootp_scrollbar(HDC hdc)
{
    if (hdc == NULL || g_kbo_hub_scrollbar_rect.right <= g_kbo_hub_scrollbar_rect.left) {
        return;
    }

    kbo_hub_update_scrollbar_geometry();

    RECT track = g_kbo_hub_scrollbar_rect;
    track.top    = g_kbo_hub_scrollbar_less_rect.bottom;
    track.bottom = g_kbo_hub_scrollbar_more_rect.top;
    if (track.bottom > track.top) {
        kbo_hub_draw_vertical_three_piece(
            hdc,
            g_kbo_hub_asset_sb_bar_top,
            g_kbo_hub_asset_sb_bar_mid,
            g_kbo_hub_asset_sb_bar_bottom,
            &track);
    }
    kbo_hub_draw_bitmap_alpha(hdc, g_kbo_hub_asset_sb_less, &g_kbo_hub_scrollbar_less_rect);
    kbo_hub_draw_bitmap_alpha(hdc, g_kbo_hub_asset_sb_more, &g_kbo_hub_scrollbar_more_rect);
    kbo_hub_draw_vertical_three_piece(
        hdc,
        g_kbo_hub_asset_sb_slider_top,
        g_kbo_hub_asset_sb_slider_mid,
        g_kbo_hub_asset_sb_slider_bottom,
        &g_kbo_hub_scrollbar_thumb_rect);
}

