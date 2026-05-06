static int kbo_hub_read_style_int(const char* path, const char* key, int default_value)
{
    if (path == NULL || key == NULL) {
        return default_value;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return default_value;
    }

    char line[256];
    int expect_value = 0;
    int result = default_value;
    while (fgets(line, sizeof(line), file) != NULL) {
        kbo_hub_trim_ascii(line);
        if (line[0] == '\0') {
            continue;
        }
        if (expect_value) {
            result = atoi(line);
            break;
        }
        if (strcmp(line, key) == 0) {
            expect_value = 1;
        }
    }

    fclose(file);
    return result;
}

static const char* kbo_hub_foreign_slot_code_for_player(uint8_t* player)
{
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            return "REPL";
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id != 0u) {
            int is_replacement = 0;
            kbo_ensure_foreign_injury_replacements_loaded();
            kbo_lock_foreign_injury_replacements();
            for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
                KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
                if (rec->replacement_player_id == player_id
                        && kbo_foreign_injury_status_uses_slot(rec->status)) {
                    is_replacement = 1;
                    break;
                }
            }
            kbo_unlock_foreign_injury_replacements();
            if (is_replacement) {
                return "REPL";
            }
        }
    }
    return kbo_player_is_asian_quota_candidate(player) ? "AQ" : "REG";
}

static void kbo_hub_load_skin_metrics(void)
{
    if (InterlockedCompareExchange(&g_kbo_hub_skin_metrics_loaded, 1, 0) != 0) {
        return;
    }

    g_kbo_hub_skin_article_gap_x = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\article\\background.ss",
        "border_left_gap",
        g_kbo_hub_skin_article_gap_x);
    g_kbo_hub_skin_article_gap_y = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\article\\background.ss",
        "border_top_gap",
        g_kbo_hub_skin_article_gap_y);
    g_kbo_hub_skin_article_font_px = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\article\\background.ss",
        "font_x_sz",
        g_kbo_hub_skin_article_font_px);
    g_kbo_hub_skin_button_font_px = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\article\\button.ss",
        "font_x_sz",
        g_kbo_hub_skin_button_font_px);
    g_kbo_hub_skin_scrollbar_width = kbo_hub_read_style_int(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\table_scrollbar\\scrollbar.ss",
        "scrollbar_button_width",
        g_kbo_hub_skin_scrollbar_width);
}

static void kbo_hub_skin_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\images\\%s",
        file_name);
}

static void kbo_hub_skin_scrollbar_image_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\skins\\ootp dark\\style_sets\\table_scrollbar\\images\\%s",
        file_name);
}

static void kbo_hub_local_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Users\\user\\OOTP27-KBO-Launcher\\assets\\icons\\%s",
        file_name);
}

static void kbo_hub_logo_asset_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0) {
        return;
    }
    snprintf(
        out,
        out_size,
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\logos\\%s",
        file_name);
}

static void kbo_hub_delete_bitmap(HBITMAP* bitmap)
{
    if (bitmap != NULL && *bitmap != NULL) {
        DeleteObject(*bitmap);
        *bitmap = NULL;
    }
}

static HBITMAP kbo_hub_load_png_hbitmap_wic(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    WCHAR wide_path[MAX_PATH];
    if (!kbo_utf8_to_wide(path, wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0])))) {
        return NULL;
    }

    IWICImagingFactory* factory = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    HBITMAP bitmap = NULL;

    HRESULT hr = CoCreateInstance(
        &CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory,
        (void**)&factory);
    if (FAILED(hr) || factory == NULL) {
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory, wide_path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || decoder == NULL) {
        goto cleanup;
    }

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || frame == NULL) {
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr) || converter == NULL) {
        goto cleanup;
    }

    hr = IWICFormatConverter_Initialize(
        converter, (IWICBitmapSource*)frame,
        &GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        goto cleanup;
    }

    UINT width = 0;
    UINT height = 0;
    hr = IWICBitmapSource_GetSize((IWICBitmapSource*)converter, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        goto cleanup;
    }

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = (LONG)width;
    bmi.bmiHeader.biHeight      = -(LONG)height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pixels = NULL;
    bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (bitmap == NULL || pixels == NULL) {
        bitmap = NULL;
        goto cleanup;
    }

    UINT stride = width * 4u;
    UINT buffer_size = stride * height;
    hr = IWICBitmapSource_CopyPixels((IWICBitmapSource*)converter, NULL, stride, buffer_size, (BYTE*)pixels);
    if (FAILED(hr)) {
        DeleteObject(bitmap);
        bitmap = NULL;
    }

cleanup:
    if (converter != NULL) { IWICFormatConverter_Release(converter); }
    if (frame     != NULL) { IWICBitmapFrameDecode_Release(frame);   }
    if (decoder   != NULL) { IWICBitmapDecoder_Release(decoder);     }
    if (factory   != NULL) { IWICImagingFactory_Release(factory);    }
    return bitmap;
}

static void kbo_hub_load_skin_assets(void)
{
    if (InterlockedCompareExchange(&g_kbo_hub_skin_assets_loaded, 1, 0) != 0) {
        return;
    }

    char path[MAX_PATH];

    kbo_hub_local_asset_path("github-mark.png", path, sizeof(path));
    g_kbo_hub_asset_github = kbo_hub_load_png_hbitmap_wic(path);
    append_logf("KBO F2 hub github asset path=%s loaded=%d", path, g_kbo_hub_asset_github != NULL);

    kbo_hub_skin_image_path("menu_arrow_right.png", path, sizeof(path));
    g_kbo_hub_asset_menu_arrow = kbo_hub_load_png_hbitmap_wic(path);

    kbo_hub_skin_scrollbar_image_path("sb_bar_top.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_top = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_bar_mid.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_mid = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_bar_bottom.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_bottom = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_less_up.png", path, sizeof(path));
    g_kbo_hub_asset_sb_less = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_more_up.png", path, sizeof(path));
    g_kbo_hub_asset_sb_more = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_top.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_top = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_mid.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_mid = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_bottom.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_bottom = kbo_hub_load_png_hbitmap_wic(path);
}

static void kbo_hub_delete_skin_assets(void)
{
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_github);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_menu_arrow);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_top);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_mid);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_bottom);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_less);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_more);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_top);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_mid);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_bottom);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_league_logo);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_team_logo);
    g_kbo_hub_logo_cache_league_id = 0;
    g_kbo_hub_logo_cache_team_id   = 0;
    g_kbo_hub_logo_cache_year      = 0;
    g_kbo_hub_skin_assets_loaded   = 0;
}

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
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS)
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

static void kbo_window_text_appendf(KboWindowTextBuffer* buffer, const char* format, ...)
{
    if (buffer == NULL || buffer->data == NULL || buffer->capacity == 0
            || buffer->length >= buffer->capacity - 1 || format == NULL) {
        return;
    }

    va_list args;
    va_start(args, format);
    int wrote = vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length, format, args);
    va_end(args);

    if (wrote <= 0) {
        return;
    }

    size_t available = buffer->capacity - buffer->length;
    if ((size_t)wrote >= available) {
        buffer->length = buffer->capacity - 1;
        buffer->data[buffer->length] = '\0';
        return;
    }

    buffer->length += (size_t)wrote;
}

static void kbo_hub_init_gdi_objects(void)
{
    kbo_hub_load_skin_metrics();
    kbo_hub_load_skin_assets();

    if (g_kbo_hub_brush_bg         == NULL) { g_kbo_hub_brush_bg         = CreateSolidBrush(KBO_HUB_COLOR_BG);         }
    if (g_kbo_hub_brush_header     == NULL) { g_kbo_hub_brush_header     = CreateSolidBrush(KBO_HUB_COLOR_HEADER);     }
    if (g_kbo_hub_brush_panel      == NULL) { g_kbo_hub_brush_panel      = CreateSolidBrush(KBO_HUB_COLOR_PANEL);      }
    if (g_kbo_hub_brush_panel_alt  == NULL) { g_kbo_hub_brush_panel_alt  = CreateSolidBrush(KBO_HUB_COLOR_PANEL_ALT);  }
    if (g_kbo_hub_brush_nav        == NULL) { g_kbo_hub_brush_nav        = CreateSolidBrush(KBO_HUB_COLOR_NAV);        }
    if (g_kbo_hub_brush_nav_active == NULL) { g_kbo_hub_brush_nav_active = CreateSolidBrush(KBO_HUB_COLOR_NAV_ACTIVE); }
    if (g_kbo_hub_brush_accent     == NULL) { g_kbo_hub_brush_accent     = CreateSolidBrush(KBO_HUB_COLOR_ACCENT);     }

    if (g_kbo_hub_font_title == NULL) {
        g_kbo_hub_font_title = CreateFontW(
            -(g_kbo_hub_skin_article_font_px + 8), 0, 0, 0,
            FW_BOLD, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
    }
    if (g_kbo_hub_font_body == NULL) {
        g_kbo_hub_font_body = CreateFontW(
            -(g_kbo_hub_skin_article_font_px - 1), 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
    }
    if (g_kbo_hub_font_mono == NULL) {
        g_kbo_hub_font_mono = CreateFontW(
            -(g_kbo_hub_skin_article_font_px - 2), 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
    }
    if (g_kbo_hub_font_small == NULL) {
        g_kbo_hub_font_small = CreateFontW(
            -(g_kbo_hub_skin_button_font_px - 4), 0, 0, 0,
            FW_BOLD, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
    }
}

static void kbo_hub_delete_gdi_objects(void)
{
    if (g_kbo_hub_font_title    != NULL) { DeleteObject(g_kbo_hub_font_title);    g_kbo_hub_font_title    = NULL; }
    if (g_kbo_hub_font_body     != NULL) { DeleteObject(g_kbo_hub_font_body);     g_kbo_hub_font_body     = NULL; }
    if (g_kbo_hub_font_mono     != NULL) { DeleteObject(g_kbo_hub_font_mono);     g_kbo_hub_font_mono     = NULL; }
    if (g_kbo_hub_font_small    != NULL) { DeleteObject(g_kbo_hub_font_small);    g_kbo_hub_font_small    = NULL; }
    if (g_kbo_hub_brush_bg         != NULL) { DeleteObject(g_kbo_hub_brush_bg);         g_kbo_hub_brush_bg         = NULL; }
    if (g_kbo_hub_brush_header     != NULL) { DeleteObject(g_kbo_hub_brush_header);     g_kbo_hub_brush_header     = NULL; }
    if (g_kbo_hub_brush_panel      != NULL) { DeleteObject(g_kbo_hub_brush_panel);      g_kbo_hub_brush_panel      = NULL; }
    if (g_kbo_hub_brush_panel_alt  != NULL) { DeleteObject(g_kbo_hub_brush_panel_alt);  g_kbo_hub_brush_panel_alt  = NULL; }
    if (g_kbo_hub_brush_nav        != NULL) { DeleteObject(g_kbo_hub_brush_nav);        g_kbo_hub_brush_nav        = NULL; }
    if (g_kbo_hub_brush_nav_active != NULL) { DeleteObject(g_kbo_hub_brush_nav_active); g_kbo_hub_brush_nav_active = NULL; }
    if (g_kbo_hub_brush_accent     != NULL) { DeleteObject(g_kbo_hub_brush_accent);     g_kbo_hub_brush_accent     = NULL; }
    kbo_hub_delete_skin_assets();
}

static BOOL CALLBACK kbo_enum_main_window_proc(HWND hwnd, LPARAM lparam)
{
    KboMainWindowSearch* search = (KboMainWindowSearch*)lparam;
    if (search == NULL || hwnd == NULL || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != NULL) {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->pid) {
        return TRUE;
    }

    if (GetWindowTextLengthA(hwnd) <= 0) {
        return TRUE;
    }

    search->hwnd = hwnd;
    return FALSE;
}

static HWND kbo_find_ootp_main_window(void)
{
    KboMainWindowSearch search;
    memset(&search, 0, sizeof(search));
    search.pid = GetCurrentProcessId();
    EnumWindows(kbo_enum_main_window_proc, (LPARAM)&search);
    return search.hwnd;
}

static int kbo_foreground_is_this_process(void)
{
    HWND foreground = GetForegroundWindow();
    if (foreground == NULL) {
        return 0;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    return pid == GetCurrentProcessId();
}

static void kbo_show_or_hide_hotkey_window(void)
{
    HWND hwnd = g_kbo_hotkey_window;
    if (hwnd == NULL || !IsWindow(hwnd)) {
        return;
    }

    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
        return;
    }

    HWND owner = kbo_find_ootp_main_window();
    if (owner != NULL) {
        SetWindowLongPtrA(hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner);
    }

    kbo_refresh_hotkey_window();
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
}

static int kbo_queue_hotkey_window_toggle(void)
{
    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_hotkey_last_toggle_ms < 250u) {
        return 1;
    }
    g_kbo_hotkey_last_toggle_ms = now;

    HWND hwnd = g_kbo_hotkey_window;
    if (hwnd == NULL || !IsWindow(hwnd)) {
        return 0;
    }

    PostMessageA(hwnd, KBO_WM_TOGGLE_SERVICE_MONITOR, 0, 0);
    return 1;
}

typedef int (__fastcall *OotpGlobalHandleCallbackFn)(void* sender, intptr_t callback_id, uintptr_t value);
typedef void* (__fastcall *OotpOperatorNewFn)(size_t size);
typedef void* (__fastcall *OotpPlayerPageCtorFn)(void* page, uint32_t player_id, int show_history, int add_to_history);
typedef void* (__fastcall *OotpPlayerProfilePageCtorFn)(void* page, uint32_t player_id, int show_history, int add_to_history);
typedef void (__fastcall *OotpUiOpenPageFn)(void* ui_context, void* page, int flags);
typedef int (__fastcall *OotpUiCurrentModeFn)(void* ui_state, int index);
typedef void* (__fastcall *OotpUiCurrentPageFn)(void* ui_state, int index);
typedef int (__fastcall *OotpPageHandleCallbackFn)(void* page, void* sender, intptr_t callback_id, uintptr_t value);
typedef int (__fastcall *OotpGlobalHandleCallbackOriginalFn)(uintptr_t sender, intptr_t callback_id, uintptr_t value);

static void kbo_get_player_profile_ui_snapshot(HMODULE exe, int* out_mode0, void** out_page0, void** out_context);

__declspec(noinline) int ootp_kbo_global_handle_callback_probe_wrapper(
    uintptr_t sender, intptr_t callback_id, uintptr_t value, uintptr_t original_func_ptr)
{
    OotpGlobalHandleCallbackOriginalFn original =
        (OotpGlobalHandleCallbackOriginalFn)original_func_ptr;
    int should_log = (callback_id == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
        || callback_id == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE);
    if (should_log) {
        append_logf("KBO player profile callback probe enter sender=%p callback=%lld value=%p",
            (void*)sender, (long long)callback_id, (void*)value);
    }
    int result = original(sender, callback_id, value);
    if (should_log) {
        append_logf("KBO player profile callback probe leave sender=%p callback=%lld value=%p result=%d",
            (void*)sender, (long long)callback_id, (void*)value, result);
    }
    return result;
}

static void* kbo_get_ootp_ui_slot_pointer(HMODULE exe, uintptr_t slot_offset, const char* label)
{
    if (exe == NULL) {
        return NULL;
    }

    uintptr_t root_addr = (uintptr_t)exe + OOTP27_UI_CONTEXT_ROOT_RVA;
    uintptr_t active_addr = (uintptr_t)exe + OOTP27_UI_CONTEXT_ACTIVE_INDEX_RVA;
    uintptr_t fallback_addr = (uintptr_t)exe + OOTP27_UI_CONTEXT_FALLBACK_INDEX_RVA;
    if (!memory_range_readable((void*)root_addr, sizeof(uintptr_t))
            || !memory_range_readable((void*)active_addr, sizeof(int32_t))
            || !memory_range_readable((void*)fallback_addr, sizeof(int16_t))) {
        append_logf("KBO player profile ui context probe: globals unreadable root=0x%x active=0x%x fallback=0x%x",
            OOTP27_UI_CONTEXT_ROOT_RVA, OOTP27_UI_CONTEXT_ACTIVE_INDEX_RVA, OOTP27_UI_CONTEXT_FALLBACK_INDEX_RVA);
        return NULL;
    }

    uintptr_t root = *(uintptr_t*)root_addr;
    int32_t index = *(int32_t*)active_addr;
    if (index == -1) {
        index = (int32_t)(*(int16_t*)fallback_addr);
    }
    if (root == 0 || index < 0 || index > 64) {
        append_logf("KBO player profile ui context probe: invalid root=%p index=%d",
            (void*)root, (int)index);
        return NULL;
    }

    uintptr_t slot = root + ((uintptr_t)(uint32_t)index * 0x558u);
    if (!memory_range_readable((void*)(slot + 0x548u), sizeof(int32_t))) {
        append_logf("KBO player profile ui context probe: slot unreadable root=%p index=%d slot=%p",
            (void*)root, (int)index, (void*)slot);
        return NULL;
    }

    int32_t sub_index = *(int32_t*)(slot + 0x548u);
    if (sub_index < 0 || sub_index > 64) {
        append_logf("KBO player profile ui context probe: invalid sub_index=%d root=%p index=%d",
            (int)sub_index, (void*)root, (int)index);
        return NULL;
    }

    uintptr_t context_slot = slot + ((uintptr_t)(uint32_t)sub_index * 0x40u) + slot_offset;
    if (!memory_range_readable((void*)context_slot, sizeof(uintptr_t))) {
        append_logf("KBO player profile ui %s probe: slot unreadable slot=%p sub=%d addr=%p",
            label != NULL ? label : "ptr", (void*)slot, (int)sub_index, (void*)context_slot);
        return NULL;
    }

    uintptr_t context = *(uintptr_t*)context_slot;
    if (context == 0 || !memory_range_readable((void*)context, 0x18u)) {
        append_logf("KBO player profile ui %s probe: invalid root=%p index=%d sub=%d ptr=%p",
            label != NULL ? label : "ptr", (void*)root, (int)index, (int)sub_index, (void*)context);
        return NULL;
    }

    append_logf("KBO player profile ui %s probe: root=%p index=%d sub=%d ptr=%p",
        label != NULL ? label : "ptr", (void*)root, (int)index, (int)sub_index, (void*)context);
    return (void*)context;
}

static void kbo_log_ootp_ui_slot_candidates(HMODULE exe)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t root_addr = (uintptr_t)exe + OOTP27_UI_CONTEXT_ROOT_RVA;
    uintptr_t active_index_addr = (uintptr_t)exe + OOTP27_UI_CONTEXT_ACTIVE_INDEX_RVA;
    uintptr_t fallback_index_addr = (uintptr_t)exe + OOTP27_UI_CONTEXT_FALLBACK_INDEX_RVA;
    if (!memory_range_readable((void*)root_addr, sizeof(uintptr_t))
        || !memory_range_readable((void*)active_index_addr, sizeof(int32_t))
        || !memory_range_readable((void*)fallback_index_addr, sizeof(int16_t))) {
        append_log_line("KBO player profile ui slot candidates: globals unreadable");
        return;
    }

    uintptr_t root = *(uintptr_t*)root_addr;
    int32_t index = *(int32_t*)active_index_addr;
    if (index == -1) {
        index = (int32_t)*(int16_t*)fallback_index_addr;
    }
    if (root == 0 || index < 0 || index > 64 || !memory_range_readable((void*)root, 0x20u)) {
        append_logf("KBO player profile ui slot candidates: invalid root=%p index=%d",
            (void*)root, (int)index);
        return;
    }

    uintptr_t slot = root + ((uintptr_t)(uint32_t)index * 0x558u);
    if (!memory_range_readable((void*)slot, 0x558u)) {
        append_logf("KBO player profile ui slot candidates: slot unreadable root=%p index=%d slot=%p",
            (void*)root, (int)index, (void*)slot);
        return;
    }

    int32_t sub_index = *(int32_t*)(slot + 0x548u);
    if (sub_index < 0 || sub_index > 64) {
        append_logf("KBO player profile ui slot candidates: invalid sub_index=%d root=%p index=%d slot=%p",
            (int)sub_index, (void*)root, (int)index, (void*)slot);
        return;
    }

    uintptr_t subslot = slot + ((uintptr_t)(uint32_t)sub_index * 0x40u);
    append_logf("KBO player profile ui slot candidates: root=%p index=%d slot=%p sub=%d subslot=%p",
        (void*)root, (int)index, (void*)slot, (int)sub_index, (void*)subslot);

    for (uintptr_t off = 0; off <= 0x58u; off += sizeof(uintptr_t)) {
        uintptr_t addr = subslot + off;
        if (!memory_range_readable((void*)addr, sizeof(uintptr_t))) {
            append_logf("KBO player profile ui slot candidate off=0x%02Ix addr=%p unreadable",
                (size_t)off, (void*)addr);
            continue;
        }

        uintptr_t ptr = *(uintptr_t*)addr;
        uintptr_t vtbl = 0;
        uintptr_t p10 = 0;
        int32_t p0c = 0;
        int readable = (ptr != 0 && memory_range_readable((void*)ptr, 0x18u));
        if (readable) {
            vtbl = *(uintptr_t*)ptr;
            p0c = *(int32_t*)(ptr + 0x0cu);
            p10 = *(uintptr_t*)(ptr + 0x10u);
        }

        append_logf("KBO player profile ui slot candidate off=0x%02Ix ptr=%p readable=%d vtbl=%p field0c=%d field10=%p",
            (size_t)off, (void*)ptr, readable, (void*)vtbl, p0c, (void*)p10);
    }
}

static void* kbo_get_ootp_current_ui_context(HMODULE exe)
{
    return kbo_get_ootp_ui_slot_pointer(exe, 0x38u, "page");
}

static void* kbo_get_ootp_current_ui_opener(HMODULE exe)
{
    return kbo_get_ootp_ui_slot_pointer(exe, 0x30u, "opener");
}

static void kbo_log_ootp_ui_open_target(const char* label, void* target)
{
    if (target == NULL || !memory_range_readable(target, 0x18u)) {
        append_logf("KBO player profile %s target invalid target=%p",
            label != NULL ? label : "open", target);
        return;
    }

    uintptr_t base = (uintptr_t)target;
    uintptr_t vtbl = *(uintptr_t*)base;
    int32_t field0c = *(int32_t*)(base + 0x0cu);
    uintptr_t field10 = *(uintptr_t*)(base + 0x10u);
    append_logf("KBO player profile %s target=%p vtbl=%p field0c=%d field10=%p",
        label != NULL ? label : "open", target, (void*)vtbl, field0c, (void*)field10);
}

static void kbo_log_ootp_callback_sender_candidate(const char* label, const char* field, void* sender)
{
    if (sender == NULL || !memory_range_readable(sender, 0x130u)) {
        append_logf("KBO player profile %s %s sender invalid sender=%p",
            label != NULL ? label : "page",
            field != NULL ? field : "candidate",
            sender);
        return;
    }

    uintptr_t base = (uintptr_t)sender;
    append_logf("KBO player profile %s %s sender=%p vtbl=%p s048=%p s050=%p s058=%p s060=%p s068=%lld s070=%p s0f8=%p s100=%p s104=%u s108=%p s110=%p s118=%p s120=%p s128=%p",
        label != NULL ? label : "page",
        field != NULL ? field : "candidate",
        sender,
        (void*)*(uintptr_t*)base,
        (void*)*(uintptr_t*)(base + 0x048u),
        (void*)*(uintptr_t*)(base + 0x050u),
        (void*)*(uintptr_t*)(base + 0x058u),
        (void*)*(uintptr_t*)(base + 0x060u),
        (long long)*(int64_t*)(base + 0x068u),
        (void*)*(uintptr_t*)(base + 0x070u),
        (void*)*(uintptr_t*)(base + 0x0f8u),
        (void*)*(uintptr_t*)(base + 0x100u),
        *(uint32_t*)(base + 0x104u),
        (void*)*(uintptr_t*)(base + 0x108u),
        (void*)*(uintptr_t*)(base + 0x110u),
        (void*)*(uintptr_t*)(base + 0x118u),
        (void*)*(uintptr_t*)(base + 0x120u),
        (void*)*(uintptr_t*)(base + 0x128u));
}

static void kbo_log_ootp_page_callback_candidates(const char* label, void* page)
{
    if (page == NULL || !memory_range_readable(page, 0x130u)) {
        append_logf("KBO player profile %s callback candidates invalid page=%p",
            label != NULL ? label : "page", page);
        return;
    }

    uintptr_t base = (uintptr_t)page;
    uintptr_t vtbl = *(uintptr_t*)base;
    append_logf("KBO player profile %s callback candidates page=%p vtbl=%p f0f8=%p f100=%p f108=%p f110=%p f118=%p f120=%p f128=%p",
        label != NULL ? label : "page",
        page,
        (void*)vtbl,
        (void*)*(uintptr_t*)(base + 0x0f8u),
        (void*)*(uintptr_t*)(base + 0x100u),
        (void*)*(uintptr_t*)(base + 0x108u),
        (void*)*(uintptr_t*)(base + 0x110u),
        (void*)*(uintptr_t*)(base + 0x118u),
        (void*)*(uintptr_t*)(base + 0x120u),
        (void*)*(uintptr_t*)(base + 0x128u));

    kbo_log_ootp_callback_sender_candidate(label, "f0f8", (void*)*(uintptr_t*)(base + 0x0f8u));
    kbo_log_ootp_callback_sender_candidate(label, "f100", (void*)*(uintptr_t*)(base + 0x100u));
    kbo_log_ootp_callback_sender_candidate(label, "f108", (void*)*(uintptr_t*)(base + 0x108u));
}

static int kbo_open_ootp_player_profile_direct_with_target(HMODULE exe, uint32_t player_id, void* open_target, const char* label)
{
    if (open_target == NULL) {
        return 0;
    }

    OotpOperatorNewFn ootp_new = (OotpOperatorNewFn)((uintptr_t)exe + OOTP27_UI_OPERATOR_NEW_RVA);
    OotpPlayerProfilePageCtorFn player_profile_page_ctor =
        (OotpPlayerProfilePageCtorFn)((uintptr_t)exe + OOTP27_PLAYER_PROFILE_PAGE_CTOR_RVA);
    OotpUiOpenPageFn open_page = (OotpUiOpenPageFn)((uintptr_t)exe + OOTP27_UI_OPEN_PAGE_RVA);

    void* page = ootp_new(0x938u);
    if (page == NULL) {
        append_logf("KBO player profile direct open failed: allocation player=%u", player_id);
        return 0;
    }

    kbo_log_ootp_ui_open_target(label, open_target);
    append_logf("KBO player profile direct open %s: constructing profile player=%u page=%p target=%p",
        label != NULL ? label : "experiment", player_id, page, open_target);
    player_profile_page_ctor(page, player_id, 1, 1);
    kbo_log_ootp_ui_open_target("new page", page);
    append_logf("KBO player profile direct open %s: installing profile player=%u page=%p target=%p flags=0",
        label != NULL ? label : "experiment", player_id, page, open_target);
    open_page(open_target, page, 0);
    append_logf("KBO player profile direct open %s: installed profile player=%u page=%p target=%p flags=0",
        label != NULL ? label : "experiment", player_id, page, open_target);
    return 1;
}

static int kbo_open_ootp_player_profile_direct_v2(HMODULE exe, uint32_t player_id)
{
    void* opener = kbo_get_ootp_current_ui_opener(exe);
    return kbo_open_ootp_player_profile_direct_with_target(exe, player_id, opener, "v2");
}

static int kbo_open_ootp_player_profile_direct_v3(HMODULE exe, uint32_t player_id)
{
    void* page_target = kbo_get_ootp_current_ui_context(exe);
    return kbo_open_ootp_player_profile_direct_with_target(exe, player_id, page_target, "v3");
}

static void kbo_log_player_profile_page_fields_v26(const char* source, uint32_t player_id, void* page, const char* stage)
{
    if (page == NULL || !memory_range_readable(page, 0x1a8u)) {
        append_logf("KBO player profile direct install v26 page fields skipped source=%s stage=%s player=%u page=%p",
            source != NULL ? source : "",
            stage != NULL ? stage : "",
            player_id,
            page);
        return;
    }

    uintptr_t base = (uintptr_t)page;
    append_logf("KBO player profile direct install v26 page fields source=%s stage=%s player=%u page=%p vtbl=%p p008=%p p00c=%d p010=%p p018=%p p020=%p p028=%p p030=%p p038=%p p040=%p p048=%p p050=%p p058=%p p060=%p p0f8=%p p100=%p p104=%u p138=%p p148=%p p190=%p p1a0=%p",
        source != NULL ? source : "",
        stage != NULL ? stage : "",
        player_id,
        page,
        (void*)*(uintptr_t*)base,
        (void*)*(uintptr_t*)(base + 0x008u),
        *(int32_t*)(base + 0x00cu),
        (void*)*(uintptr_t*)(base + 0x010u),
        (void*)*(uintptr_t*)(base + 0x018u),
        (void*)*(uintptr_t*)(base + 0x020u),
        (void*)*(uintptr_t*)(base + 0x028u),
        (void*)*(uintptr_t*)(base + 0x030u),
        (void*)*(uintptr_t*)(base + 0x038u),
        (void*)*(uintptr_t*)(base + 0x040u),
        (void*)*(uintptr_t*)(base + 0x048u),
        (void*)*(uintptr_t*)(base + 0x050u),
        (void*)*(uintptr_t*)(base + 0x058u),
        (void*)*(uintptr_t*)(base + 0x060u),
        (void*)*(uintptr_t*)(base + 0x0f8u),
        (void*)*(uintptr_t*)(base + 0x100u),
        *(uint32_t*)(base + 0x104u),
        (void*)*(uintptr_t*)(base + 0x138u),
        (void*)*(uintptr_t*)(base + 0x148u),
        (void*)*(uintptr_t*)(base + 0x190u),
        (void*)*(uintptr_t*)(base + 0x1a0u));
}

static int kbo_open_ootp_player_page_direct_install_v26(HMODULE exe, uint32_t player_id, const char* source)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_direct_install_v26_last_ms < 900u) {
        append_logf("KBO player profile direct install v26 skipped: cooldown source=%s player=%u elapsed_ms=%llu",
            source != NULL ? source : "", player_id,
            (unsigned long long)(now - g_kbo_player_profile_direct_install_v26_last_ms));
        return 0;
    }
    g_kbo_player_profile_direct_install_v26_last_ms = now;

    void* opener = kbo_get_ootp_current_ui_context(exe);
    uintptr_t expected_ui_state = (uintptr_t)exe + OOTP27_UI_CONTEXT_ROOT_RVA - 0x1610u;
    if (opener == NULL || !memory_range_readable(opener, 0x18u)) {
        append_logf("KBO player profile direct install v26 skipped: opener invalid source=%s player=%u opener=%p",
            source != NULL ? source : "", player_id, opener);
        return 0;
    }

    int32_t opener_index = *(int32_t*)((uintptr_t)opener + 0x0cu);
    uintptr_t opener_ui_state = *(uintptr_t*)((uintptr_t)opener + 0x10u);
    uintptr_t opener_vtbl = *(uintptr_t*)opener;
    if (opener_index < 0 || opener_index > 64 || opener_ui_state != expected_ui_state) {
        append_logf("KBO player profile direct install v26 skipped: opener fields unexpected source=%s player=%u opener=%p vtbl=%p index=%d ui_state=%p expected=%p",
            source != NULL ? source : "", player_id, opener, (void*)opener_vtbl,
            (int)opener_index, (void*)opener_ui_state, (void*)expected_ui_state);
        return 0;
    }

    int before_mode0 = -1;
    int after_mode0 = -1;
    void* before_page0 = NULL;
    void* after_page0 = NULL;
    void* before_context = NULL;
    void* after_context = NULL;
    kbo_get_player_profile_ui_snapshot(exe, &before_mode0, &before_page0, &before_context);

    OotpOperatorNewFn ootp_new = (OotpOperatorNewFn)((uintptr_t)exe + OOTP27_UI_OPERATOR_NEW_RVA);
    OotpPlayerPageCtorFn player_page_ctor =
        (OotpPlayerPageCtorFn)((uintptr_t)exe + OOTP27_PLAYER_PAGE_CTOR_RVA);
    OotpUiOpenPageFn open_page = (OotpUiOpenPageFn)((uintptr_t)exe + OOTP27_UI_OPEN_PAGE_RVA);

    void* page = ootp_new(0x1a8u);
    if (page == NULL) {
        append_logf("KBO player profile direct install v26 failed: allocation source=%s player=%u",
            source != NULL ? source : "", player_id);
        return 0;
    }

    append_logf("KBO player profile direct install v26 constructing source=%s player=%u opener=%p opener_vtbl=%p index=%d page=%p before_mode0=%d before_page0=%p before_context=%p",
        source != NULL ? source : "", player_id, opener, (void*)opener_vtbl, (int)opener_index,
        page, before_mode0, before_page0, before_context);
    void* constructed = player_page_ctor(page, player_id, 1, 1);
    kbo_log_player_profile_page_fields_v26(source, player_id, page, "after_ctor");
    append_logf("KBO player profile direct install v26 installing source=%s player=%u page=%p constructed=%p opener=%p flags=0",
        source != NULL ? source : "", player_id, page, constructed, opener);
    open_page(opener, page, 0);
    kbo_log_player_profile_page_fields_v26(source, player_id, page, "after_open_page");
    kbo_get_player_profile_ui_snapshot(exe, &after_mode0, &after_page0, &after_context);
    append_logf("KBO player profile direct install v26 result source=%s player=%u page=%p opener=%p after_mode0=%d after_page0=%p after_context=%p changed=%d/%d",
        source != NULL ? source : "", player_id, page, opener,
        after_mode0, after_page0, after_context,
        after_page0 != before_page0 ? 1 : 0,
        after_context != before_context ? 1 : 0);
    if (after_page0 == before_page0 && after_context == before_context) {
        kbo_log_ootp_ui_slot_candidates(exe);
    }
    return after_page0 != before_page0 || after_context != before_context;
}

static int kbo_try_player_profile_page_callback_v4(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    if (InterlockedCompareExchange(&g_kbo_player_profile_direct_attempted, 1, 0) != 0) {
        append_logf("KBO player profile page callback v4 skipped: one-shot already used player=%u",
            player_id);
        return 0;
    }

    void* page = kbo_get_ootp_current_ui_context(exe);
    if (page == NULL) {
        append_logf("KBO player profile page callback v4 skipped: no current page player=%u",
            player_id);
        return 0;
    }

    kbo_log_ootp_ui_open_target("v4 current page", page);
    append_logf("KBO player profile page callback v4: dispatching current-page callback player=%u page=%p callback=-0x3ed",
        player_id, page);

    OotpPageHandleCallbackFn page_handle_callback =
        (OotpPageHandleCallbackFn)((uintptr_t)exe + OOTP27_PAGE_HANDLE_CALLBACK_RVA);
    int result = page_handle_callback(page, page, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU, (uintptr_t)player_id);
    append_logf("KBO player profile page callback v4: result=%d player=%u page=%p",
        result, player_id, page);
    return result != 0;
}

static int kbo_try_player_profile_global_open_v5(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    if (InterlockedCompareExchange(&g_kbo_player_profile_direct_attempted, 1, 0) != 0) {
        append_logf("KBO player profile global open v5 skipped: one-shot already used player=%u",
            player_id);
        return 0;
    }

    OotpGlobalHandleCallbackFn global_handle_callback =
        (OotpGlobalHandleCallbackFn)((uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA);
    append_logf("KBO player profile global open v5: dispatching callback player=%u sender=NULL callback=-0x3ee",
        player_id);
    int result = global_handle_callback(NULL, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN, (uintptr_t)player_id);
    append_logf("KBO player profile global open v5: result=%d player=%u sender=NULL callback=-0x3ee",
        result, player_id);
    return result != 0;
}

static int kbo_try_player_profile_global_open_v6(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v6_last_ms < 500u) {
        append_logf("KBO player profile global open v6 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v6_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v6_last_ms = now;

    void* page = kbo_get_ootp_current_ui_context(exe);
    if (page == NULL || !memory_range_readable(page, 0x100u)) {
        append_logf("KBO player profile global open v6 skipped: current page invalid player=%u page=%p",
            player_id, page);
        return 0;
    }

    void* sender = (void*)*(uintptr_t*)((uintptr_t)page + 0x0f8u);
    if (sender == NULL || !memory_range_readable(sender, 0x108u)) {
        append_logf("KBO player profile global open v6 skipped: sender invalid player=%u page=%p sender=%p",
            player_id, page, sender);
        return 0;
    }

    OotpGlobalHandleCallbackFn global_handle_callback =
        (OotpGlobalHandleCallbackFn)((uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA);
    append_logf("KBO player profile global open v6: dispatching callback player=%u page=%p sender=%p callback=-0x3ee sender104=%u",
        player_id, page, sender, *(uint32_t*)((uintptr_t)sender + 0x104u));
    int result = global_handle_callback(sender, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN, (uintptr_t)player_id);
    append_logf("KBO player profile global open v6: result=%d player=%u page=%p sender=%p callback=-0x3ee",
        result, player_id, page, sender);
    return result != 0;
}

static int kbo_player_profile_global_open_v7_sender_valid(void* sender)
{
    if (sender == NULL || !memory_range_readable(sender, 0x108u)) {
        return 0;
    }

    uintptr_t vtbl = *(uintptr_t*)sender;
    return vtbl != 0u && memory_range_readable((void*)vtbl, sizeof(uintptr_t));
}

static int kbo_try_player_profile_global_open_v7(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v7_last_ms < 500u) {
        append_logf("KBO player profile global open v7 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v7_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v7_last_ms = now;

    void* page = kbo_get_ootp_current_ui_context(exe);
    void* current_sender = NULL;
    if (page != NULL && memory_range_readable(page, 0x100u)) {
        current_sender = (void*)*(uintptr_t*)((uintptr_t)page + 0x0f8u);
    }

    void* sender = NULL;
    const char* source = "none";
    if (kbo_player_profile_global_open_v7_sender_valid(current_sender)) {
        sender = current_sender;
        source = "current";
        g_kbo_player_profile_global_open_v7_cached_sender = (uintptr_t)current_sender;
    } else {
        void* cached_sender = (void*)g_kbo_player_profile_global_open_v7_cached_sender;
        if (kbo_player_profile_global_open_v7_sender_valid(cached_sender)) {
            sender = cached_sender;
            source = "cached";
        }
    }

    if (sender == NULL) {
        append_logf("KBO player profile global open v7 skipped: no sender player=%u page=%p current_sender=%p cached_sender=%p",
            player_id, page, current_sender, (void*)g_kbo_player_profile_global_open_v7_cached_sender);
        return 0;
    }

    OotpGlobalHandleCallbackFn global_handle_callback =
        (OotpGlobalHandleCallbackFn)((uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA);
    append_logf("KBO player profile global open v7: dispatching callback player=%u page=%p sender=%p source=%s callback=-0x3ee sender104=%u",
        player_id, page, sender, source, *(uint32_t*)((uintptr_t)sender + 0x104u));
    int result = global_handle_callback(sender, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN, (uintptr_t)player_id);
    append_logf("KBO player profile global open v7: result=%d player=%u page=%p sender=%p source=%s callback=-0x3ee",
        result, player_id, page, sender, source);
    return result != 0;
}

static int kbo_try_player_profile_global_open_v8(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v8_last_ms < 700u) {
        append_logf("KBO player profile global open v8 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v8_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v8_last_ms = now;

    void* page = kbo_get_ootp_current_ui_context(exe);
    void* current_sender = NULL;
    if (page != NULL && memory_range_readable(page, 0x100u)) {
        current_sender = (void*)*(uintptr_t*)((uintptr_t)page + 0x0f8u);
    }

    void* sender = NULL;
    const char* source = "none";
    if (kbo_player_profile_global_open_v7_sender_valid(current_sender)) {
        sender = current_sender;
        source = "current";
        g_kbo_player_profile_global_open_v7_cached_sender = (uintptr_t)current_sender;
    } else {
        void* cached_sender = (void*)g_kbo_player_profile_global_open_v7_cached_sender;
        if (kbo_player_profile_global_open_v7_sender_valid(cached_sender)) {
            sender = cached_sender;
            source = "cached";
        }
    }

    if (sender == NULL) {
        append_logf("KBO player profile global open v8 skipped: no sender player=%u page=%p current_sender=%p cached_sender=%p",
            player_id, page, current_sender, (void*)g_kbo_player_profile_global_open_v7_cached_sender);
        return 0;
    }

    OotpGlobalHandleCallbackFn global_handle_callback =
        (OotpGlobalHandleCallbackFn)((uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA);
    append_logf("KBO player profile global open v8: dispatching popup player=%u page=%p sender=%p source=%s callback=-0x3ee sender104=%u",
        player_id, page, sender, source, *(uint32_t*)((uintptr_t)sender + 0x104u));
    int popup_result = global_handle_callback(sender, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN, (uintptr_t)player_id);
    append_logf("KBO player profile global open v8: popup result=%d player=%u page=%p sender=%p source=%s callback=-0x3ee",
        popup_result, player_id, page, sender, source);

    if (!kbo_player_profile_global_open_v7_sender_valid(sender)) {
        append_logf("KBO player profile global open v8 skipped menu action: sender invalid after popup player=%u sender=%p",
            player_id, sender);
        return popup_result != 0;
    }

    append_logf("KBO player profile global open v8: dispatching menu action player=%u page=%p sender=%p source=%s callback=-0x3ed sender104=%u",
        player_id, page, sender, source, *(uint32_t*)((uintptr_t)sender + 0x104u));
    int menu_result = global_handle_callback(sender, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU, (uintptr_t)player_id);
    append_logf("KBO player profile global open v8: menu action result=%d player=%u page=%p sender=%p source=%s callback=-0x3ed",
        menu_result, player_id, page, sender, source);
    return menu_result != 0 || popup_result != 0;
}

static void kbo_get_player_profile_ui_snapshot(HMODULE exe, int* out_mode0, void** out_page0, void** out_context)
{
    if (out_mode0 != NULL) {
        *out_mode0 = -1;
    }
    if (out_page0 != NULL) {
        *out_page0 = NULL;
    }
    if (out_context != NULL) {
        *out_context = NULL;
    }
    if (exe == NULL) {
        return;
    }

    uintptr_t ui_state = (uintptr_t)exe + OOTP27_UI_CONTEXT_ROOT_RVA - 0x1610u;
    if (memory_range_readable((void*)ui_state, 0x40u)) {
        OotpUiCurrentModeFn current_mode = (OotpUiCurrentModeFn)((uintptr_t)exe + OOTP27_UI_CURRENT_MODE_RVA);
        OotpUiCurrentPageFn current_page = (OotpUiCurrentPageFn)((uintptr_t)exe + OOTP27_UI_CURRENT_PAGE_RVA);
        if (out_mode0 != NULL) {
            *out_mode0 = current_mode((void*)ui_state, 0);
        }
        if (out_page0 != NULL) {
            *out_page0 = current_page((void*)ui_state, 0);
        }
    }
    if (out_context != NULL) {
        *out_context = kbo_get_ootp_current_ui_context(exe);
    }
}

static void kbo_log_player_profile_id_lookup(uint32_t player_id, const char* source)
{
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        append_logf("KBO player profile id lookup unavailable source=%s player=%u",
            source != NULL ? source : "", player_id);
        return;
    }

    uintptr_t as_index_ptr = 0;
    uint32_t as_index_field_id = 0;
    if (player_id != 0u && player_id <= (uint32_t)player_count) {
        as_index_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)(player_id - 1u) * sizeof(uintptr_t)));
        if (kbo_player_pointer_plausible(as_index_ptr)) {
            as_index_field_id = *(uint32_t*)((uint8_t*)as_index_ptr + OOTP27_PLAYER_ID_OFFSET);
        }
    }

    uint32_t found_index = 0;
    uintptr_t found_ptr = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint32_t candidate_id = *(uint32_t*)((uint8_t*)player_ptr + OOTP27_PLAYER_ID_OFFSET);
        if (candidate_id == player_id) {
            found_index = (uint32_t)i + 1u;
            found_ptr = player_ptr;
            break;
        }
    }

    append_logf("KBO player profile id lookup source=%s player=%u vector_off=0x%x count=%d as_index_ptr=%p as_index_field=%u found_index=%u found_ptr=%p",
        source != NULL ? source : "",
        player_id,
        vector_offset,
        player_count,
        (void*)as_index_ptr,
        as_index_field_id,
        found_index,
        (void*)found_ptr);
}

static int kbo_try_player_profile_menu_action_now(HMODULE exe, uint32_t player_id, const char* action_source)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    kbo_log_player_profile_id_lookup(player_id, action_source);

    void* page = kbo_get_ootp_current_ui_context(exe);
    void* current_sender = NULL;
    if (page != NULL && memory_range_readable(page, 0x100u)) {
        current_sender = (void*)*(uintptr_t*)((uintptr_t)page + 0x0f8u);
    }

    void* sender = NULL;
    const char* sender_source = "none";
    if (kbo_player_profile_global_open_v7_sender_valid(current_sender)) {
        sender = current_sender;
        sender_source = "current";
        g_kbo_player_profile_global_open_v7_cached_sender = (uintptr_t)current_sender;
    } else {
        void* cached_sender = (void*)g_kbo_player_profile_global_open_v7_cached_sender;
        if (kbo_player_profile_global_open_v7_sender_valid(cached_sender)) {
            sender = cached_sender;
            sender_source = "cached";
        }
    }

    if (sender == NULL) {
        append_logf("KBO player profile menu action skipped: no sender source=%s player=%u page=%p current_sender=%p cached_sender=%p",
            action_source != NULL ? action_source : "", player_id, page, current_sender,
            (void*)g_kbo_player_profile_global_open_v7_cached_sender);
        return 0;
    }

    OotpGlobalHandleCallbackFn global_handle_callback =
        (OotpGlobalHandleCallbackFn)((uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA);

    int before_mode0 = -1;
    int after_mode0 = -1;
    int after_old_mode0 = -1;
    void* before_page0 = NULL;
    void* after_page0 = NULL;
    void* after_old_page0 = NULL;
    void* before_context = NULL;
    void* after_context = NULL;
    void* after_old_context = NULL;
    kbo_get_player_profile_ui_snapshot(exe, &before_mode0, &before_page0, &before_context);

    append_logf("KBO player profile menu action dispatch source=%s player=%u page=%p sender=%p sender_source=%s callback=-0x3ed sender104=%u before_mode0=%d before_page0=%p before_context=%p",
        action_source != NULL ? action_source : "", player_id, page, sender, sender_source,
        *(uint32_t*)((uintptr_t)sender + 0x104u), before_mode0, before_page0, before_context);
    int result = global_handle_callback(sender, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU, (uintptr_t)player_id);
    kbo_get_player_profile_ui_snapshot(exe, &after_mode0, &after_page0, &after_context);
    append_logf("KBO player profile menu action result source=%s result=%d player=%u page=%p sender=%p callback=-0x3ed after_mode0=%d after_page0=%p after_context=%p changed=%d/%d",
        action_source != NULL ? action_source : "", result, player_id, page, sender,
        after_mode0, after_page0, after_context,
        after_page0 != before_page0 ? 1 : 0,
        after_context != before_context ? 1 : 0);

    if (after_page0 != before_page0 || after_context != before_context) {
        return result != 0;
    }

    append_logf("KBO player profile menu action fallback dispatch source=%s player=%u sender=%p callback=-0x3f2",
        action_source != NULL ? action_source : "", player_id, sender);
    int old_result = global_handle_callback(sender, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE, (uintptr_t)player_id);
    kbo_get_player_profile_ui_snapshot(exe, &after_old_mode0, &after_old_page0, &after_old_context);
    append_logf("KBO player profile menu action fallback result source=%s result=%d player=%u sender=%p callback=-0x3f2 after_mode0=%d after_page0=%p after_context=%p changed=%d/%d",
        action_source != NULL ? action_source : "", old_result, player_id, sender,
        after_old_mode0, after_old_page0, after_old_context,
        after_old_page0 != before_page0 ? 1 : 0,
        after_old_context != before_context ? 1 : 0);

    if ((after_old_page0 == before_page0 && after_old_context == before_context)
            && read_kbo_localappdata_flag_file("enable_player_profile_direct_install_v26.txt")) {
        append_logf("KBO player profile menu action direct install v26 dispatch source=%s player=%u reason=callbacks_no_page_change",
            action_source != NULL ? action_source : "", player_id);
        int direct_result = kbo_open_ootp_player_page_direct_install_v26(exe, player_id, action_source);
        append_logf("KBO player profile menu action direct install v26 result source=%s player=%u result=%d",
            action_source != NULL ? action_source : "", player_id, direct_result);
        return direct_result != 0 || old_result != 0 || result != 0;
    }

    return old_result != 0 || result != 0;
}

static void* kbo_find_player_profile_menu_item(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return NULL;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    uintptr_t expected_callback = (uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA;
    uintptr_t cursor = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    void* best = NULL;
    int logged = 0;
    SIZE_T scanned = 0;

    while (VirtualQuery((void*)cursor, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= cursor) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && protect_allows_read(mbi.Protect)
                && (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED)
                && mbi.RegionSize <= (64u * 1024u * 1024u)) {
            uintptr_t scan_start = base;
            uintptr_t scan_end = end;
            if (scan_end > scan_start + (64u * 1024u * 1024u)) {
                scan_end = scan_start + (64u * 1024u * 1024u);
            }

            for (uintptr_t p = scan_start; p + 0x220u <= scan_end; p += sizeof(uintptr_t)) {
                if (*(uintptr_t*)p != expected_vtbl) {
                    continue;
                }

                uintptr_t cb40 = *(uintptr_t*)(p + 0x40u);
                uintptr_t cb48 = *(uintptr_t*)(p + 0x48u);
                uintptr_t a58 = *(uintptr_t*)(p + 0x58u);
                uintptr_t a60 = *(uintptr_t*)(p + 0x60u);
                intptr_t a68 = *(intptr_t*)(p + 0x68u);
                uintptr_t a70 = *(uintptr_t*)(p + 0x70u);
                uintptr_t parent190 = *(uintptr_t*)(p + 0x190u);

                int primary_match = cb40 == expected_callback
                    && (intptr_t)a58 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
                    && (uint32_t)a60 == player_id;
                int secondary_match = cb48 == expected_callback
                    && a68 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
                    && (uint32_t)a70 == player_id;

                if ((primary_match || secondary_match) && logged < 8) {
                    append_logf("KBO player profile menu item candidate player=%u item=%p primary=%d secondary=%d cb40=%p a58=%lld a60=%llu cb48=%p a68=%lld a70=%llu parent190=%p",
                        player_id,
                        (void*)p,
                        primary_match,
                        secondary_match,
                        (void*)cb40,
                        (long long)(intptr_t)a58,
                        (unsigned long long)a60,
                        (void*)cb48,
                        (long long)a68,
                        (unsigned long long)a70,
                        (void*)parent190);
                    logged++;
                }

                if (primary_match) {
                    best = (void*)p;
                    goto done;
                }
                if (best == NULL && secondary_match) {
                    best = (void*)p;
                }
            }
            scanned += scan_end - scan_start;
            if (scanned > (512u * 1024u * 1024u)) {
                break;
            }
        }

        cursor = end;
    }

done:
    append_logf("KBO player profile menu item scan done player=%u found=%p logged=%d scanned_mb=%llu",
        player_id, best, logged, (unsigned long long)(scanned / (1024u * 1024u)));
    return best;
}

static int kbo_try_player_profile_global_open_v9(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v9_last_ms < 900u) {
        append_logf("KBO player profile global open v9 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v9_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v9_last_ms = now;

    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    void* item = kbo_find_player_profile_menu_item(exe, player_id);
    if (item == NULL) {
        append_logf("KBO player profile global open v9 skipped: no menu item player=%u popup_opened=%d",
            player_id, popup_opened);
        return popup_opened;
    }

    OotpGlobalHandleCallbackFn global_handle_callback =
        (OotpGlobalHandleCallbackFn)((uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA);
    append_logf("KBO player profile global open v9: dispatching item action player=%u item=%p callback=-0x3ed",
        player_id, item);
    int result = global_handle_callback(item, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU, (uintptr_t)player_id);
    append_logf("KBO player profile global open v9: item action result=%d player=%u item=%p callback=-0x3ed",
        result, player_id, item);
    return result != 0 || popup_opened;
}

static void kbo_log_ootp_widget_candidate(const char* label, void* ptr, uint32_t player_id)
{
    if (label == NULL) {
        label = "widget";
    }
    if (ptr == NULL || !memory_range_readable(ptr, 0x220u)) {
        append_logf("KBO player profile %s candidate invalid player=%u ptr=%p",
            label, player_id, ptr);
        return;
    }

    uintptr_t base = (uintptr_t)ptr;
    uintptr_t vtbl = *(uintptr_t*)base;
    uintptr_t cb40 = *(uintptr_t*)(base + 0x40u);
    uintptr_t cb48 = *(uintptr_t*)(base + 0x48u);
    uintptr_t a58 = *(uintptr_t*)(base + 0x58u);
    uintptr_t a60 = *(uintptr_t*)(base + 0x60u);
    intptr_t a68 = *(intptr_t*)(base + 0x68u);
    uintptr_t a70 = *(uintptr_t*)(base + 0x70u);
    uintptr_t on_state = *(uintptr_t*)(base + 0x138u);
    uintptr_t on_state_arg = *(uintptr_t*)(base + 0x140u);
    uintptr_t on_click = *(uintptr_t*)(base + 0x148u);
    uintptr_t on_click_r8 = *(uintptr_t*)(base + 0x150u);
    uintptr_t on_click_r9 = *(uintptr_t*)(base + 0x158u);
    uintptr_t draw_fn = *(uintptr_t*)(base + 0x168u);
    uintptr_t draw_arg1 = *(uintptr_t*)(base + 0x170u);
    uintptr_t draw_arg2 = *(uintptr_t*)(base + 0x178u);
    uint32_t flags184 = *(uint32_t*)(base + 0x184u);
    uintptr_t child190 = *(uintptr_t*)(base + 0x190u);
    uintptr_t child1a8 = *(uintptr_t*)(base + 0x1a8u);
    uintptr_t extra210 = *(uintptr_t*)(base + 0x210u);

    append_logf("KBO player profile %s candidate player=%u ptr=%p vtbl=%p cb40=%p cb48=%p a58=%lld a60=%llu a68=%lld a70=%llu state=%p/%p click=%p/%p/%p draw=%p/%p/%p flags184=0x%x child190=%p child1a8=%p extra210=%p",
        label,
        player_id,
        ptr,
        (void*)vtbl,
        (void*)cb40,
        (void*)cb48,
        (long long)(intptr_t)a58,
        (unsigned long long)a60,
        (long long)a68,
        (unsigned long long)a70,
        (void*)on_state,
        (void*)on_state_arg,
        (void*)on_click,
        (void*)on_click_r8,
        (void*)on_click_r9,
        (void*)draw_fn,
        (void*)draw_arg1,
        (void*)draw_arg2,
        flags184,
        (void*)child190,
        (void*)child1a8,
        (void*)extra210);

    uintptr_t expected_callback = (uintptr_t)GetModuleHandleA(NULL) + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA;
    for (uintptr_t off = 0x20u; off <= 0x210u; off += sizeof(uintptr_t)) {
        uintptr_t value = *(uintptr_t*)(base + off);
        if (value == expected_callback
                || (intptr_t)value == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
                || (intptr_t)value == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
                || (uint32_t)value == player_id) {
            append_logf("KBO player profile %s field hit player=%u ptr=%p off=0x%Ix value=%p signed=%lld u32=%u",
                label,
                player_id,
                ptr,
                (size_t)off,
                (void*)value,
                (long long)(intptr_t)value,
                (uint32_t)value);
        }
    }
}

static void kbo_log_ootp_widget_child_ptr(const char* label, void* owner, uintptr_t off, uint32_t player_id)
{
    if (owner == NULL || !memory_range_readable(owner, off + sizeof(uintptr_t))) {
        return;
    }

    uintptr_t value = *(uintptr_t*)((uintptr_t)owner + off);
    int readable = value != 0 && memory_range_readable((void*)value, 0x220u);
    uintptr_t vtbl = 0;
    if (readable) {
        vtbl = *(uintptr_t*)value;
    }
    append_logf("KBO player profile %s child ptr player=%u owner=%p off=0x%Ix ptr=%p readable=%d vtbl=%p",
        label != NULL ? label : "widget",
        player_id,
        owner,
        (size_t)off,
        (void*)value,
        readable,
        (void*)vtbl);
    if (readable) {
        kbo_log_ootp_widget_candidate(label, (void*)value, player_id);
    }
}

static void kbo_log_ootp_widget_embedded_child(const char* label, void* owner, uintptr_t off, uint32_t player_id)
{
    if (owner == NULL || !memory_range_readable(owner, off + 0x220u)) {
        return;
    }

    void* child = (void*)((uintptr_t)owner + off);
    uintptr_t vtbl = *(uintptr_t*)child;
    if (vtbl == 0 || !memory_range_readable((void*)vtbl, sizeof(uintptr_t))) {
        return;
    }

    append_logf("KBO player profile %s embedded child player=%u owner=%p off=0x%Ix child=%p vtbl=%p",
        label != NULL ? label : "widget",
        player_id,
        owner,
        (size_t)off,
        child,
        (void*)vtbl);
    kbo_log_ootp_widget_candidate(label, child, player_id);
}

static void kbo_log_ootp_popup_owner_children(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t popup_owner_addr = (uintptr_t)exe + OOTP27_UI_POPUP_OWNER_RVA;
    if (!memory_range_readable((void*)popup_owner_addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile popup children %s skipped: popup_owner global unreadable",
            stage != NULL ? stage : "stage");
        return;
    }

    void* owner = (void*)*(uintptr_t*)popup_owner_addr;
    if (owner == NULL || !memory_range_readable(owner, 0x1400u)) {
        append_logf("KBO player profile popup children %s skipped: owner invalid player=%u owner=%p",
            stage != NULL ? stage : "stage", player_id, owner);
        return;
    }

    append_logf("KBO player profile popup children %s begin player=%u owner=%p vtbl=%p",
        stage != NULL ? stage : "stage", player_id, owner, (void*)*(uintptr_t*)owner);

    for (uintptr_t off = 0x20u; off <= 0x220u; off += sizeof(uintptr_t)) {
        uintptr_t value = *(uintptr_t*)((uintptr_t)owner + off);
        if (value != 0 && memory_range_readable((void*)value, 0x220u)) {
            kbo_log_ootp_widget_child_ptr(stage, owner, off, player_id);
        }
    }

    for (uintptr_t off = 0x230u; off <= 0x9f8u; off += 0x90u) {
        kbo_log_ootp_widget_embedded_child(stage, owner, off, player_id);
    }

    for (uintptr_t off = 0x9f8u; off <= 0x13f0u; off += sizeof(uintptr_t)) {
        uintptr_t value = *(uintptr_t*)((uintptr_t)owner + off);
        if (value != 0 && memory_range_readable((void*)value, 0x220u)) {
            kbo_log_ootp_widget_child_ptr(stage, owner, off, player_id);
        }
    }

    append_logf("KBO player profile popup children %s end player=%u owner=%p",
        stage != NULL ? stage : "stage", player_id, owner);
}

static void kbo_log_ootp_widget_vector_children(const char* label, void* container, uintptr_t vector_off, uint32_t player_id)
{
    if (container == NULL || !memory_range_readable(container, vector_off + 0x18u)) {
        return;
    }

    uintptr_t begin = *(uintptr_t*)((uintptr_t)container + vector_off);
    uintptr_t end = *(uintptr_t*)((uintptr_t)container + vector_off + sizeof(uintptr_t));
    uintptr_t cap = *(uintptr_t*)((uintptr_t)container + vector_off + sizeof(uintptr_t) * 2u);
    if (begin == 0 || end < begin || cap < end || ((end - begin) % sizeof(uintptr_t)) != 0) {
        return;
    }

    uintptr_t bytes = end - begin;
    size_t count = (size_t)(bytes / sizeof(uintptr_t));
    if (count == 0u || count > 128u || bytes > 0x400u || !memory_range_readable((void*)begin, bytes)) {
        return;
    }

    append_logf("KBO player profile v12 vector %s player=%u container=%p off=0x%Ix begin=%p end=%p cap=%p count=%Iu",
        label != NULL ? label : "container",
        player_id,
        container,
        (size_t)vector_off,
        (void*)begin,
        (void*)end,
        (void*)cap,
        count);

    size_t limit = count < 16u ? count : 16u;
    for (size_t i = 0; i < limit; i++) {
        uintptr_t elem = *(uintptr_t*)(begin + i * sizeof(uintptr_t));
        int readable = elem != 0 && memory_range_readable((void*)elem, 0x220u);
        uintptr_t vtbl = readable ? *(uintptr_t*)elem : 0;
        append_logf("KBO player profile v12 vector elem %s player=%u off=0x%Ix idx=%Iu ptr=%p readable=%d vtbl=%p",
            label != NULL ? label : "container",
            player_id,
            (size_t)vector_off,
            i,
            (void*)elem,
            readable,
            (void*)vtbl);
        if (readable) {
            kbo_log_ootp_widget_candidate(label, (void*)elem, player_id);
        }
    }
}

static void kbo_log_ootp_popup_owner_focused_children(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t popup_owner_addr = (uintptr_t)exe + OOTP27_UI_POPUP_OWNER_RVA;
    if (!memory_range_readable((void*)popup_owner_addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile v12 %s skipped: popup_owner global unreadable",
            stage != NULL ? stage : "stage");
        return;
    }

    void* owner = (void*)*(uintptr_t*)popup_owner_addr;
    if (owner == NULL || !memory_range_readable(owner, 0x1400u)) {
        append_logf("KBO player profile v12 %s skipped: owner invalid player=%u owner=%p",
            stage != NULL ? stage : "stage", player_id, owner);
        return;
    }

    append_logf("KBO player profile v12 %s begin player=%u owner=%p vtbl=%p",
        stage != NULL ? stage : "stage", player_id, owner, (void*)*(uintptr_t*)owner);

    static const uintptr_t focus_offsets[] = { 0x90u, 0x98u, 0xa0u, 0x190u, 0x1a8u, 0x210u };
    for (size_t i = 0; i < sizeof(focus_offsets) / sizeof(focus_offsets[0]); i++) {
        uintptr_t off = focus_offsets[i];
        if (!memory_range_readable((void*)((uintptr_t)owner + off), sizeof(uintptr_t))) {
            continue;
        }

        uintptr_t ptr = *(uintptr_t*)((uintptr_t)owner + off);
        int readable = ptr != 0 && memory_range_readable((void*)ptr, 0x240u);
        uintptr_t vtbl = readable ? *(uintptr_t*)ptr : 0;
        append_logf("KBO player profile v12 focus ptr %s player=%u owner=%p off=0x%Ix ptr=%p readable=%d vtbl=%p",
            stage != NULL ? stage : "stage",
            player_id,
            owner,
            (size_t)off,
            (void*)ptr,
            readable,
            (void*)vtbl);
        if (!readable) {
            continue;
        }

        kbo_log_ootp_widget_candidate(stage, (void*)ptr, player_id);
        for (uintptr_t vec_off = 0x40u; vec_off <= 0x220u; vec_off += sizeof(uintptr_t)) {
            kbo_log_ootp_widget_vector_children(stage, (void*)ptr, vec_off, player_id);
        }
    }

    append_logf("KBO player profile v12 %s end player=%u owner=%p",
        stage != NULL ? stage : "stage", player_id, owner);
}

static void kbo_log_ootp_popup_owner_rows(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t popup_owner_addr = (uintptr_t)exe + OOTP27_UI_POPUP_OWNER_RVA;
    if (!memory_range_readable((void*)popup_owner_addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile v13 %s skipped: popup_owner global unreadable",
            stage != NULL ? stage : "stage");
        return;
    }

    void* owner = (void*)*(uintptr_t*)popup_owner_addr;
    if (owner == NULL || !memory_range_readable(owner, 0x220u)) {
        append_logf("KBO player profile v13 %s skipped: owner invalid player=%u owner=%p",
            stage != NULL ? stage : "stage", player_id, owner);
        return;
    }

    uintptr_t rows = *(uintptr_t*)((uintptr_t)owner + 0xe8u);
    int16_t row_count = *(int16_t*)((uintptr_t)owner + 0xf0u);
    int16_t selected_row = *(int16_t*)((uintptr_t)owner + 0xf2u);
    int16_t active_row = *(int16_t*)((uintptr_t)owner + 0xf4u);
    append_logf("KBO player profile v13 %s begin player=%u owner=%p vtbl=%p rows=%p row_count=%d selected_row=%d active_row=%d",
        stage != NULL ? stage : "stage",
        player_id,
        owner,
        (void*)*(uintptr_t*)owner,
        (void*)rows,
        (int)row_count,
        (int)selected_row,
        (int)active_row);

    if (rows == 0 || row_count <= 0 || row_count > 64 || !memory_range_readable((void*)rows, (size_t)row_count * sizeof(uintptr_t))) {
        append_logf("KBO player profile v13 %s no readable rows player=%u rows=%p row_count=%d",
            stage != NULL ? stage : "stage", player_id, (void*)rows, (int)row_count);
        return;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    uintptr_t expected_callback = (uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA;
    for (int r = 0; r < row_count && r < 24; r++) {
        uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
        if (row == 0 || !memory_range_readable((void*)row, 0x58u)) {
            append_logf("KBO player profile v13 row %s player=%u idx=%d ptr=%p readable=0",
                stage != NULL ? stage : "stage", player_id, r, (void*)row);
            continue;
        }

        uintptr_t children = *(uintptr_t*)(row + 0x8u);
        int16_t child_count = *(int16_t*)(row + 0x10u);
        uint32_t row_height = *(uint32_t*)(row + 0x18u);
        append_logf("KBO player profile v13 row %s player=%u idx=%d row=%p children=%p child_count=%d row_height=%u",
            stage != NULL ? stage : "stage",
            player_id,
            r,
            (void*)row,
            (void*)children,
            (int)child_count,
            row_height);

        if (children == 0 || child_count <= 0 || child_count > 64 || !memory_range_readable((void*)children, (size_t)child_count * sizeof(uintptr_t))) {
            continue;
        }

        for (int c = 0; c < child_count && c < 24; c++) {
            uintptr_t child = *(uintptr_t*)(children + (size_t)c * sizeof(uintptr_t));
            int readable = child != 0 && memory_range_readable((void*)child, 0x220u);
            uintptr_t vtbl = readable ? *(uintptr_t*)child : 0;
            append_logf("KBO player profile v13 child %s player=%u row=%d col=%d child=%p readable=%d vtbl=%p menu_vtbl=%d",
                stage != NULL ? stage : "stage",
                player_id,
                r,
                c,
                (void*)child,
                readable,
                (void*)vtbl,
                vtbl == expected_vtbl);
            if (!readable) {
                continue;
            }

            kbo_log_ootp_widget_candidate(stage, (void*)child, player_id);
            uintptr_t cb40 = *(uintptr_t*)(child + 0x40u);
            uintptr_t cb48 = *(uintptr_t*)(child + 0x48u);
            uintptr_t a58 = *(uintptr_t*)(child + 0x58u);
            uintptr_t a60 = *(uintptr_t*)(child + 0x60u);
            intptr_t a68 = *(intptr_t*)(child + 0x68u);
            uintptr_t a70 = *(uintptr_t*)(child + 0x70u);
            if (vtbl == expected_vtbl
                    || cb40 == expected_callback
                    || cb48 == expected_callback
                    || (intptr_t)a58 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
                    || (uint32_t)a60 == player_id
                    || a68 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
                    || (uint32_t)a70 == player_id) {
                append_logf("KBO player profile v13 strong child %s player=%u row=%d col=%d child=%p vtbl=%p cb40=%p a58=%lld a60=%llu cb48=%p a68=%lld a70=%llu",
                    stage != NULL ? stage : "stage",
                    player_id,
                    r,
                    c,
                    (void*)child,
                    (void*)vtbl,
                    (void*)cb40,
                    (long long)(intptr_t)a58,
                    (unsigned long long)a60,
                    (void*)cb48,
                    (long long)a68,
                    (unsigned long long)a70);
            }
        }
    }

    append_logf("KBO player profile v13 %s end player=%u owner=%p",
        stage != NULL ? stage : "stage", player_id, owner);
}

static void kbo_log_ootp_possible_menu_rows(HMODULE exe, const char* stage, void* owner, uintptr_t candidate, uintptr_t source_off, uint32_t player_id)
{
    if (exe == NULL || owner == NULL || candidate == 0 || !memory_range_readable((void*)candidate, 0x220u)) {
        return;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    uintptr_t expected_callback = (uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA;
    uintptr_t vtbl = *(uintptr_t*)candidate;
    int strong_logged = 0;

    for (uintptr_t rows_off = 0x20u; rows_off <= 0x220u; rows_off += sizeof(uintptr_t)) {
        uintptr_t rows = *(uintptr_t*)(candidate + rows_off);
        if (rows == 0 || !memory_range_readable((void*)rows, sizeof(uintptr_t))) {
            continue;
        }

        int row_counts[4];
        row_counts[0] = (int)*(int16_t*)(candidate + rows_off + 0x8u);
        row_counts[1] = (int)*(int16_t*)(candidate + rows_off + 0x10u);
        row_counts[2] = (int)*(int16_t*)(candidate + rows_off + 0x18u);
        row_counts[3] = (int)*(int16_t*)(candidate + rows_off + 0x20u);

        for (int count_i = 0; count_i < 4; count_i++) {
            int row_count = row_counts[count_i];
            if (row_count <= 0 || row_count > 64 || !memory_range_readable((void*)rows, (size_t)row_count * sizeof(uintptr_t))) {
                continue;
            }

            int plausible_rows = 0;
            for (int r = 0; r < row_count && r < 8; r++) {
                uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
                if (row != 0 && memory_range_readable((void*)row, 0x58u)) {
                    uintptr_t children = *(uintptr_t*)(row + 0x8u);
                    int16_t child_count = *(int16_t*)(row + 0x10u);
                    if (children != 0 && child_count > 0 && child_count <= 64
                            && memory_range_readable((void*)children, (size_t)child_count * sizeof(uintptr_t))) {
                        plausible_rows++;
                    }
                }
            }
            if (plausible_rows == 0) {
                continue;
            }

            append_logf("KBO player profile v14 row root %s player=%u owner=%p source_off=0x%Ix root=%p vtbl=%p rows_off=0x%Ix rows=%p row_count=%d count_slot=%d plausible_rows=%d",
                stage != NULL ? stage : "stage",
                player_id,
                owner,
                (size_t)source_off,
                (void*)candidate,
                (void*)vtbl,
                (size_t)rows_off,
                (void*)rows,
                row_count,
                count_i,
                plausible_rows);

            for (int r = 0; r < row_count && r < 16; r++) {
                uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
                if (row == 0 || !memory_range_readable((void*)row, 0x58u)) {
                    continue;
                }
                uintptr_t children = *(uintptr_t*)(row + 0x8u);
                int16_t child_count = *(int16_t*)(row + 0x10u);
                uint32_t row_height = *(uint32_t*)(row + 0x18u);
                append_logf("KBO player profile v14 row %s player=%u root=%p row=%d ptr=%p children=%p child_count=%d row_height=%u",
                    stage != NULL ? stage : "stage",
                    player_id,
                    (void*)candidate,
                    r,
                    (void*)row,
                    (void*)children,
                    (int)child_count,
                    row_height);
                if (children == 0 || child_count <= 0 || child_count > 64
                        || !memory_range_readable((void*)children, (size_t)child_count * sizeof(uintptr_t))) {
                    continue;
                }

                for (int c = 0; c < child_count && c < 16; c++) {
                    uintptr_t child = *(uintptr_t*)(children + (size_t)c * sizeof(uintptr_t));
                    int readable = child != 0 && memory_range_readable((void*)child, 0x220u);
                    uintptr_t child_vtbl = readable ? *(uintptr_t*)child : 0;
                    append_logf("KBO player profile v14 child %s player=%u root=%p row=%d col=%d child=%p readable=%d vtbl=%p menu_vtbl=%d",
                        stage != NULL ? stage : "stage",
                        player_id,
                        (void*)candidate,
                        r,
                        c,
                        (void*)child,
                        readable,
                        (void*)child_vtbl,
                        child_vtbl == expected_vtbl);
                    if (!readable) {
                        continue;
                    }

                    uintptr_t cb40 = *(uintptr_t*)(child + 0x40u);
                    uintptr_t cb48 = *(uintptr_t*)(child + 0x48u);
                    uintptr_t a58 = *(uintptr_t*)(child + 0x58u);
                    uintptr_t a60 = *(uintptr_t*)(child + 0x60u);
                    intptr_t a68 = *(intptr_t*)(child + 0x68u);
                    uintptr_t a70 = *(uintptr_t*)(child + 0x70u);
                    if (child_vtbl == expected_vtbl
                            || cb40 == expected_callback
                            || cb48 == expected_callback
                            || (intptr_t)a58 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
                            || (uint32_t)a60 == player_id
                            || a68 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
                            || (uint32_t)a70 == player_id) {
                        append_logf("KBO player profile v14 strong child %s player=%u root=%p row=%d col=%d child=%p vtbl=%p cb40=%p a58=%lld a60=%llu cb48=%p a68=%lld a70=%llu",
                            stage != NULL ? stage : "stage",
                            player_id,
                            (void*)candidate,
                            r,
                            c,
                            (void*)child,
                            (void*)child_vtbl,
                            (void*)cb40,
                            (long long)(intptr_t)a58,
                            (unsigned long long)a60,
                            (void*)cb48,
                            (long long)a68,
                            (unsigned long long)a70);
                        strong_logged++;
                    }
                }
            }
        }
    }

    if (strong_logged > 0) {
        append_logf("KBO player profile v14 candidate done %s player=%u owner=%p source_off=0x%Ix root=%p strong_logged=%d",
            stage != NULL ? stage : "stage",
            player_id,
            owner,
            (size_t)source_off,
            (void*)candidate,
            strong_logged);
    }
}

static void kbo_log_ootp_popup_owner_row_candidates(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t popup_owner_addr = (uintptr_t)exe + OOTP27_UI_POPUP_OWNER_RVA;
    if (!memory_range_readable((void*)popup_owner_addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile v14 %s skipped: popup_owner global unreadable",
            stage != NULL ? stage : "stage");
        return;
    }

    void* owner = (void*)*(uintptr_t*)popup_owner_addr;
    if (owner == NULL || !memory_range_readable(owner, 0x1400u)) {
        append_logf("KBO player profile v14 %s skipped: owner invalid player=%u owner=%p",
            stage != NULL ? stage : "stage", player_id, owner);
        return;
    }

    append_logf("KBO player profile v14 %s begin player=%u owner=%p vtbl=%p",
        stage != NULL ? stage : "stage", player_id, owner, (void*)*(uintptr_t*)owner);

    kbo_log_ootp_possible_menu_rows(exe, stage, owner, (uintptr_t)owner, 0, player_id);
    for (uintptr_t off = 0x20u; off <= 0x13e8u; off += sizeof(uintptr_t)) {
        uintptr_t ptr = *(uintptr_t*)((uintptr_t)owner + off);
        if (ptr != 0 && memory_range_readable((void*)ptr, 0x220u)) {
            kbo_log_ootp_possible_menu_rows(exe, stage, owner, ptr, off, player_id);
        }
    }

    append_logf("KBO player profile v14 %s end player=%u owner=%p",
        stage != NULL ? stage : "stage", player_id, owner);
}

static void kbo_log_ootp_popup_global_link(HMODULE exe, const char* stage, const char* name, uintptr_t rva, uint32_t player_id)
{
    if (exe == NULL || name == NULL) {
        return;
    }

    uintptr_t addr = (uintptr_t)exe + rva;
    if (!memory_range_readable((void*)addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile v15 %s global %s unreadable addr=%p rva=0x%Ix",
            stage != NULL ? stage : "stage", name, (void*)addr, (size_t)rva);
        return;
    }

    uintptr_t ptr = *(uintptr_t*)addr;
    int readable = ptr != 0 && memory_range_readable((void*)ptr, 0x220u);
    uintptr_t vtbl = readable ? *(uintptr_t*)ptr : 0;
    uintptr_t p90 = readable ? *(uintptr_t*)(ptr + 0x90u) : 0;
    uintptr_t p98 = readable ? *(uintptr_t*)(ptr + 0x98u) : 0;
    append_logf("KBO player profile v15 %s global %s addr=%p ptr=%p readable=%d vtbl=%p p90=%p p98=%p",
        stage != NULL ? stage : "stage",
        name,
        (void*)addr,
        (void*)ptr,
        readable,
        (void*)vtbl,
        (void*)p90,
        (void*)p98);

    if (!readable) {
        return;
    }

    kbo_log_ootp_widget_candidate(name, (void*)ptr, player_id);
    kbo_log_ootp_possible_menu_rows(exe, stage, (void*)ptr, ptr, 0, player_id);
    if (p90 != 0 && memory_range_readable((void*)p90, 0x220u)) {
        kbo_log_ootp_widget_candidate(name, (void*)p90, player_id);
        kbo_log_ootp_possible_menu_rows(exe, stage, (void*)ptr, p90, 0x90u, player_id);
    }
    if (p98 != 0 && memory_range_readable((void*)p98, 0x220u)) {
        kbo_log_ootp_widget_candidate(name, (void*)p98, player_id);
        kbo_log_ootp_possible_menu_rows(exe, stage, (void*)ptr, p98, 0x98u, player_id);
    }
}

static void kbo_log_ootp_popup_globals_v15(HMODULE exe, const char* stage, uint32_t player_id)
{
    kbo_log_ootp_popup_global_link(exe, stage, "popup_owner", OOTP27_UI_POPUP_OWNER_RVA, player_id);
    kbo_log_ootp_popup_global_link(exe, stage, "popup_host", OOTP27_UI_POPUP_HOST_RVA, player_id);
    kbo_log_ootp_popup_global_link(exe, stage, "popup_stack_root", OOTP27_UI_POPUP_STACK_ROOT_RVA, player_id);
    kbo_log_ootp_popup_global_link(exe, stage, "popup_stack_active", OOTP27_UI_POPUP_STACK_ACTIVE_RVA, player_id);
    kbo_log_ootp_popup_global_link(exe, stage, "popup_stack_child", OOTP27_UI_POPUP_STACK_CHILD_RVA, player_id);
}

static void kbo_log_ootp_pointer_block_v16(HMODULE exe, const char* stage, const char* label, uintptr_t ptr, uint32_t player_id, int depth)
{
    if (exe == NULL || label == NULL || ptr == 0 || depth < 0 || !memory_range_readable((void*)ptr, 0x240u)) {
        return;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    uintptr_t expected_callback = (uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA;
    uintptr_t vtbl = *(uintptr_t*)ptr;
    append_logf("KBO player profile v16 block %s %s player=%u ptr=%p vtbl=%p menu_vtbl=%d depth=%d",
        stage != NULL ? stage : "stage",
        label,
        player_id,
        (void*)ptr,
        (void*)vtbl,
        vtbl == expected_vtbl,
        depth);
    kbo_log_ootp_widget_candidate(label, (void*)ptr, player_id);
    kbo_log_ootp_possible_menu_rows(exe, stage, (void*)ptr, ptr, 0, player_id);

    for (uintptr_t off = 0x20u; off <= 0x238u; off += sizeof(uintptr_t)) {
        uintptr_t value = *(uintptr_t*)(ptr + off);
        if (value == 0) {
            continue;
        }

        int readable = memory_range_readable((void*)value, 0x220u);
        uintptr_t child_vtbl = readable ? *(uintptr_t*)value : 0;
        uintptr_t raw = value;
        int interesting = readable
            || raw == expected_callback
            || (intptr_t)raw == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
            || (intptr_t)raw == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
            || (uint32_t)raw == player_id;
        if (!interesting) {
            continue;
        }

        append_logf("KBO player profile v16 field %s %s player=%u ptr=%p off=0x%Ix value=%p readable=%d vtbl=%p menu_vtbl=%d signed=%lld u32=%u",
            stage != NULL ? stage : "stage",
            label,
            player_id,
            (void*)ptr,
            (size_t)off,
            (void*)value,
            readable,
            (void*)child_vtbl,
            child_vtbl == expected_vtbl,
            (long long)(intptr_t)value,
            (uint32_t)value);

        if (readable) {
            uintptr_t cb40 = *(uintptr_t*)(value + 0x40u);
            uintptr_t cb48 = *(uintptr_t*)(value + 0x48u);
            uintptr_t a58 = *(uintptr_t*)(value + 0x58u);
            uintptr_t a60 = *(uintptr_t*)(value + 0x60u);
            intptr_t a68 = *(intptr_t*)(value + 0x68u);
            uintptr_t a70 = *(uintptr_t*)(value + 0x70u);
            if (child_vtbl == expected_vtbl
                    || cb40 == expected_callback
                    || cb48 == expected_callback
                    || (intptr_t)a58 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
                    || (uint32_t)a60 == player_id
                    || a68 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
                    || (uint32_t)a70 == player_id) {
                append_logf("KBO player profile v16 strong field %s %s player=%u parent=%p off=0x%Ix child=%p vtbl=%p cb40=%p a58=%lld a60=%llu cb48=%p a68=%lld a70=%llu",
                    stage != NULL ? stage : "stage",
                    label,
                    player_id,
                    (void*)ptr,
                    (size_t)off,
                    (void*)value,
                    (void*)child_vtbl,
                    (void*)cb40,
                    (long long)(intptr_t)a58,
                    (unsigned long long)a60,
                    (void*)cb48,
                    (long long)a68,
                    (unsigned long long)a70);
            }
        }
    }

    if (depth == 0) {
        return;
    }

    static const uintptr_t next_offsets[] = { 0x90u, 0x98u, 0x190u, 0x1a8u, 0x210u };
    for (size_t i = 0; i < sizeof(next_offsets) / sizeof(next_offsets[0]); i++) {
        uintptr_t off = next_offsets[i];
        uintptr_t next = *(uintptr_t*)(ptr + off);
        if (next != 0 && memory_range_readable((void*)next, 0x220u) && next != ptr) {
            char child_label[96];
            snprintf(child_label, sizeof(child_label), "%s+0x%llx", label, (unsigned long long)off);
            kbo_log_ootp_pointer_block_v16(exe, stage, child_label, next, player_id, depth - 1);
        }
    }
}

static void kbo_log_ootp_popup_host_deep_v16(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t host_addr = (uintptr_t)exe + OOTP27_UI_POPUP_HOST_RVA;
    if (!memory_range_readable((void*)host_addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile v16 %s skipped: popup_host global unreadable addr=%p",
            stage != NULL ? stage : "stage", (void*)host_addr);
        return;
    }

    uintptr_t host = *(uintptr_t*)host_addr;
    append_logf("KBO player profile v16 %s begin player=%u popup_host=%p",
        stage != NULL ? stage : "stage", player_id, (void*)host);
    kbo_log_ootp_pointer_block_v16(exe, stage, "popup_host", host, player_id, 2);
    append_logf("KBO player profile v16 %s end player=%u popup_host=%p",
        stage != NULL ? stage : "stage", player_id, (void*)host);
}

static void kbo_log_ootp_menu_item_detail_v17(HMODULE exe, const char* stage, const char* label, uintptr_t item, uint32_t player_id)
{
    if (exe == NULL || label == NULL || item == 0 || !memory_range_readable((void*)item, 0x1400u)) {
        return;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    uintptr_t expected_callback = (uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA;
    uintptr_t vtbl = *(uintptr_t*)item;
    uintptr_t cb40 = *(uintptr_t*)(item + 0x40u);
    uintptr_t cb48 = *(uintptr_t*)(item + 0x48u);
    uintptr_t a58 = *(uintptr_t*)(item + 0x58u);
    uintptr_t a60 = *(uintptr_t*)(item + 0x60u);
    intptr_t a68 = *(intptr_t*)(item + 0x68u);
    uintptr_t a70 = *(uintptr_t*)(item + 0x70u);
    uintptr_t p90 = *(uintptr_t*)(item + 0x90u);
    uintptr_t p98 = *(uintptr_t*)(item + 0x98u);
    uintptr_t pe8 = *(uintptr_t*)(item + 0xe8u);
    uintptr_t pf0 = *(uintptr_t*)(item + 0xf0u);
    uintptr_t p190 = *(uintptr_t*)(item + 0x190u);
    uintptr_t p1a8 = *(uintptr_t*)(item + 0x1a8u);
    uintptr_t p210 = *(uintptr_t*)(item + 0x210u);

    append_logf("KBO player profile v17 menu detail %s %s player=%u item=%p vtbl=%p menu_vtbl=%d cb40=%p cb48=%p a58=%lld a60=%llu a68=%lld a70=%llu p90=%p p98=%p pe8=%p pf0=%p p190=%p p1a8=%p p210=%p",
        stage != NULL ? stage : "stage",
        label,
        player_id,
        (void*)item,
        (void*)vtbl,
        vtbl == expected_vtbl,
        (void*)cb40,
        (void*)cb48,
        (long long)(intptr_t)a58,
        (unsigned long long)a60,
        (long long)a68,
        (unsigned long long)a70,
        (void*)p90,
        (void*)p98,
        (void*)pe8,
        (void*)pf0,
        (void*)p190,
        (void*)p1a8,
        (void*)p210);

    for (uintptr_t off = 0x20u; off <= 0x13f0u; off += sizeof(uintptr_t)) {
        uintptr_t value = *(uintptr_t*)(item + off);
        if (value == 0) {
            continue;
        }

        int readable = memory_range_readable((void*)value, 0x80u);
        uintptr_t child_vtbl = readable ? *(uintptr_t*)value : 0;
        int interesting = child_vtbl == expected_vtbl
            || value == expected_callback
            || (intptr_t)value == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
            || (intptr_t)value == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
            || (uint32_t)value == player_id;
        if (!interesting) {
            continue;
        }

        append_logf("KBO player profile v17 menu field %s %s player=%u item=%p off=0x%Ix value=%p readable=%d vtbl=%p menu_vtbl=%d signed=%lld u32=%u",
            stage != NULL ? stage : "stage",
            label,
            player_id,
            (void*)item,
            (size_t)off,
            (void*)value,
            readable,
            (void*)child_vtbl,
            child_vtbl == expected_vtbl,
            (long long)(intptr_t)value,
            (uint32_t)value);
    }
}

static void kbo_log_ootp_menu_children_v17(HMODULE exe, const char* stage, const char* label, uintptr_t root, uint32_t player_id)
{
    if (exe == NULL || label == NULL || root == 0 || !memory_range_readable((void*)root, 0x820u)) {
        append_logf("KBO player profile v17 menu children %s %s skipped root=%p",
            stage != NULL ? stage : "stage", label != NULL ? label : "root", (void*)root);
        return;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    append_logf("KBO player profile v17 menu children %s %s begin player=%u root=%p root_vtbl=%p",
        stage != NULL ? stage : "stage",
        label,
        player_id,
        (void*)root,
        (void*)*(uintptr_t*)root);

    int found = 0;
    for (uintptr_t off = 0x20u; off <= 0x800u; off += sizeof(uintptr_t)) {
        uintptr_t child = *(uintptr_t*)(root + off);
        if (child == 0 || !memory_range_readable((void*)child, 0x1400u)) {
            continue;
        }
        uintptr_t child_vtbl = *(uintptr_t*)child;
        if (child_vtbl != expected_vtbl) {
            continue;
        }

        found++;
        char child_label[96];
        snprintf(child_label, sizeof(child_label), "%s+0x%llx", label, (unsigned long long)off);
        append_logf("KBO player profile v17 menu child %s %s player=%u root=%p off=0x%Ix child=%p",
            stage != NULL ? stage : "stage",
            label,
            player_id,
            (void*)root,
            (size_t)off,
            (void*)child);
        kbo_log_ootp_menu_item_detail_v17(exe, stage, child_label, child, player_id);
    }

    append_logf("KBO player profile v17 menu children %s %s end player=%u root=%p found=%d",
        stage != NULL ? stage : "stage", label, player_id, (void*)root, found);
}

static void kbo_log_ootp_popup_host_menu_probe_v17(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t host_addr = (uintptr_t)exe + OOTP27_UI_POPUP_HOST_RVA;
    if (!memory_range_readable((void*)host_addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile v17 %s skipped: popup_host global unreadable addr=%p",
            stage != NULL ? stage : "stage", (void*)host_addr);
        return;
    }

    uintptr_t host = *(uintptr_t*)host_addr;
    if (host == 0 || !memory_range_readable((void*)host, 0x240u)) {
        append_logf("KBO player profile v17 %s skipped: popup_host invalid host=%p",
            stage != NULL ? stage : "stage", (void*)host);
        return;
    }

    uintptr_t root190 = *(uintptr_t*)(host + 0x190u);
    uintptr_t root1a8 = *(uintptr_t*)(host + 0x1a8u);
    uintptr_t root210 = *(uintptr_t*)(host + 0x210u);
    append_logf("KBO player profile v17 %s host roots player=%u host=%p vtbl=%p r190=%p r1a8=%p r210=%p",
        stage != NULL ? stage : "stage",
        player_id,
        (void*)host,
        (void*)*(uintptr_t*)host,
        (void*)root190,
        (void*)root1a8,
        (void*)root210);

    kbo_log_ootp_menu_children_v17(exe, stage, "popup_host", host, player_id);
    kbo_log_ootp_menu_children_v17(exe, stage, "popup_host+0x190", root190, player_id);
    kbo_log_ootp_menu_children_v17(exe, stage, "popup_host+0x1a8", root1a8, player_id);
    kbo_log_ootp_menu_children_v17(exe, stage, "popup_host+0x210", root210, player_id);
}

static void kbo_log_ootp_popup_stack_probe_v18(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    static const struct {
        const char* name;
        uintptr_t rva;
    } roots[] = {
        { "popup_owner", OOTP27_UI_POPUP_OWNER_RVA },
        { "popup_host", OOTP27_UI_POPUP_HOST_RVA },
        { "popup_stack_active", OOTP27_UI_POPUP_STACK_ACTIVE_RVA },
        { "popup_stack_child", OOTP27_UI_POPUP_STACK_CHILD_RVA },
    };

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        uintptr_t addr = (uintptr_t)exe + roots[i].rva;
        if (!memory_range_readable((void*)addr, sizeof(uintptr_t))) {
            append_logf("KBO player profile v18 %s global %s unreadable addr=%p",
                stage != NULL ? stage : "stage", roots[i].name, (void*)addr);
            continue;
        }

        uintptr_t ptr = *(uintptr_t*)addr;
        uintptr_t vtbl = (ptr != 0 && memory_range_readable((void*)ptr, 0x240u)) ? *(uintptr_t*)ptr : 0;
        append_logf("KBO player profile v18 %s global %s player=%u addr=%p ptr=%p vtbl=%p menu_vtbl=%d",
            stage != NULL ? stage : "stage",
            roots[i].name,
            player_id,
            (void*)addr,
            (void*)ptr,
            (void*)vtbl,
            vtbl == expected_vtbl);

        if (ptr == 0 || !memory_range_readable((void*)ptr, 0x240u)) {
            continue;
        }

        if (vtbl == expected_vtbl) {
            kbo_log_ootp_menu_item_detail_v17(exe, stage, roots[i].name, ptr, player_id);
        }

        kbo_log_ootp_menu_children_v17(exe, stage, roots[i].name, ptr, player_id);
        kbo_log_ootp_pointer_block_v16(exe, stage, roots[i].name, ptr, player_id, 2);
    }
}

static void kbo_log_ootp_popup_stack_rows_v19(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t active_addr = (uintptr_t)exe + OOTP27_UI_POPUP_STACK_ACTIVE_RVA;
    if (!memory_range_readable((void*)active_addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile v19 %s skipped: popup_stack_active global unreadable addr=%p",
            stage != NULL ? stage : "stage", (void*)active_addr);
        return;
    }

    uintptr_t active = *(uintptr_t*)active_addr;
    if (active == 0 || !memory_range_readable((void*)active, 0x240u)) {
        append_logf("KBO player profile v19 %s skipped: popup_stack_active invalid player=%u active=%p",
            stage != NULL ? stage : "stage", player_id, (void*)active);
        return;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    uintptr_t expected_callback = (uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA;
    uintptr_t rows = *(uintptr_t*)(active + 0x1e0u);
    int16_t counts[4];
    counts[0] = *(int16_t*)(active + 0x1e8u);
    counts[1] = *(int16_t*)(active + 0x1f0u);
    counts[2] = *(int16_t*)(active + 0x1f8u);
    counts[3] = *(int16_t*)(active + 0x200u);

    append_logf("KBO player profile v19 %s active rows player=%u active=%p vtbl=%p rows=%p c0=%d c1=%d c2=%d c3=%d",
        stage != NULL ? stage : "stage",
        player_id,
        (void*)active,
        (void*)*(uintptr_t*)active,
        (void*)rows,
        (int)counts[0],
        (int)counts[1],
        (int)counts[2],
        (int)counts[3]);

    int row_count = 0;
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
        if (counts[i] > row_count && counts[i] <= 64) {
            row_count = counts[i];
        }
    }
    if (rows == 0 || row_count <= 0 || !memory_range_readable((void*)rows, (size_t)row_count * sizeof(uintptr_t))) {
        append_logf("KBO player profile v19 %s skipped: rows invalid player=%u rows=%p row_count=%d",
            stage != NULL ? stage : "stage", player_id, (void*)rows, row_count);
        return;
    }

    for (int r = 0; r < row_count && r < 20; r++) {
        uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
        if (row == 0 || !memory_range_readable((void*)row, 0xa0u)) {
            append_logf("KBO player profile v19 row %s player=%u active=%p row=%d ptr=%p readable=0",
                stage != NULL ? stage : "stage", player_id, (void*)active, r, (void*)row);
            continue;
        }

        append_logf("KBO player profile v19 row %s player=%u active=%p row=%d ptr=%p vtbl=%p q00=%p q08=%p q10=%p q18=%p q20=%p q28=%p q30=%p q38=%p q40=%p q48=%p q50=%p q58=%p q60=%p q68=%p q70=%p q78=%p",
            stage != NULL ? stage : "stage",
            player_id,
            (void*)active,
            r,
            (void*)row,
            (void*)*(uintptr_t*)row,
            (void*)*(uintptr_t*)(row + 0x00u),
            (void*)*(uintptr_t*)(row + 0x08u),
            (void*)*(uintptr_t*)(row + 0x10u),
            (void*)*(uintptr_t*)(row + 0x18u),
            (void*)*(uintptr_t*)(row + 0x20u),
            (void*)*(uintptr_t*)(row + 0x28u),
            (void*)*(uintptr_t*)(row + 0x30u),
            (void*)*(uintptr_t*)(row + 0x38u),
            (void*)*(uintptr_t*)(row + 0x40u),
            (void*)*(uintptr_t*)(row + 0x48u),
            (void*)*(uintptr_t*)(row + 0x50u),
            (void*)*(uintptr_t*)(row + 0x58u),
            (void*)*(uintptr_t*)(row + 0x60u),
            (void*)*(uintptr_t*)(row + 0x68u),
            (void*)*(uintptr_t*)(row + 0x70u),
            (void*)*(uintptr_t*)(row + 0x78u));

        for (uintptr_t off = 0x00u; off <= 0x80u; off += sizeof(uintptr_t)) {
            uintptr_t candidate = *(uintptr_t*)(row + off);
            if (candidate == 0 || !memory_range_readable((void*)candidate, 0x80u)) {
                continue;
            }
            uintptr_t first = *(uintptr_t*)candidate;
            uintptr_t second = memory_range_readable((void*)(candidate + 0x8u), sizeof(uintptr_t))
                ? *(uintptr_t*)(candidate + 0x8u)
                : 0;
            int16_t maybe_count0 = memory_range_readable((void*)(row + off + 0x8u), sizeof(int16_t))
                ? *(int16_t*)(row + off + 0x8u)
                : 0;
            int16_t maybe_count1 = memory_range_readable((void*)(row + off + 0x10u), sizeof(int16_t))
                ? *(int16_t*)(row + off + 0x10u)
                : 0;

            append_logf("KBO player profile v19 row field %s player=%u row=%d off=0x%Ix ptr=%p first=%p second=%p menu_vtbl=%d count8=%d count10=%d",
                stage != NULL ? stage : "stage",
                player_id,
                r,
                (size_t)off,
                (void*)candidate,
                (void*)first,
                (void*)second,
                first == expected_vtbl,
                (int)maybe_count0,
                (int)maybe_count1);

            if (first == expected_vtbl) {
                kbo_log_ootp_menu_item_detail_v17(exe, stage, "v19-row-field-menu", candidate, player_id);
            }

            if (maybe_count0 > 0 && maybe_count0 <= 32
                    && memory_range_readable((void*)candidate, (size_t)maybe_count0 * sizeof(uintptr_t))) {
                for (int c = 0; c < maybe_count0 && c < 16; c++) {
                    uintptr_t child = *(uintptr_t*)(candidate + (size_t)c * sizeof(uintptr_t));
                    int readable = child != 0 && memory_range_readable((void*)child, 0x220u);
                    uintptr_t child_vtbl = readable ? *(uintptr_t*)child : 0;
                    append_logf("KBO player profile v19 vec child %s player=%u row=%d off=0x%Ix col=%d child=%p readable=%d vtbl=%p menu_vtbl=%d",
                        stage != NULL ? stage : "stage",
                        player_id,
                        r,
                        (size_t)off,
                        c,
                        (void*)child,
                        readable,
                        (void*)child_vtbl,
                        child_vtbl == expected_vtbl);
                    if (!readable) {
                        continue;
                    }
                    uintptr_t cb40 = *(uintptr_t*)(child + 0x40u);
                    uintptr_t cb48 = *(uintptr_t*)(child + 0x48u);
                    uintptr_t a58 = *(uintptr_t*)(child + 0x58u);
                    uintptr_t a60 = *(uintptr_t*)(child + 0x60u);
                    intptr_t a68 = *(intptr_t*)(child + 0x68u);
                    uintptr_t a70 = *(uintptr_t*)(child + 0x70u);
                    if (child_vtbl == expected_vtbl
                            || cb40 == expected_callback
                            || cb48 == expected_callback
                            || (intptr_t)a58 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
                            || (uint32_t)a60 == player_id
                            || a68 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
                            || (uint32_t)a70 == player_id) {
                        append_logf("KBO player profile v19 strong child %s player=%u row=%d off=0x%Ix col=%d child=%p vtbl=%p cb40=%p a58=%lld a60=%llu cb48=%p a68=%lld a70=%llu",
                            stage != NULL ? stage : "stage",
                            player_id,
                            r,
                            (size_t)off,
                            c,
                            (void*)child,
                            (void*)child_vtbl,
                            (void*)cb40,
                            (long long)(intptr_t)a58,
                            (unsigned long long)a60,
                            (void*)cb48,
                            (long long)a68,
                            (unsigned long long)a70);
                    }
                }
            }
        }
    }
}

static void kbo_log_ootp_popup_stack_row_candidate_v20(HMODULE exe, const char* stage, uintptr_t active, uintptr_t rows_off, uint32_t player_id)
{
    if (exe == NULL || active == 0 || !memory_range_readable((void*)active, 0x500u)) {
        return;
    }

    uintptr_t rows = *(uintptr_t*)(active + rows_off);
    if (rows == 0 || !memory_range_readable((void*)rows, sizeof(uintptr_t) * 2u)) {
        return;
    }

    int16_t counts[5] = {0};
    uintptr_t count_offsets[5] = { rows_off + 0x08u, rows_off + 0x10u, rows_off + 0x18u, rows_off + 0x20u, rows_off + 0x28u };
    int row_count = 0;
    for (int i = 0; i < 5; i++) {
        if (count_offsets[i] + sizeof(int16_t) <= 0x500u) {
            counts[i] = *(int16_t*)(active + count_offsets[i]);
            if (counts[i] > row_count && counts[i] <= 64) {
                row_count = counts[i];
            }
        }
    }

    if (row_count <= 0 || !memory_range_readable((void*)rows, (size_t)row_count * sizeof(uintptr_t))) {
        return;
    }

    int plausible_rows = 0;
    for (int r = 0; r < row_count && r < 12; r++) {
        uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
        if (row != 0 && memory_range_readable((void*)row, 0xa0u)) {
            plausible_rows++;
        }
    }
    if (plausible_rows == 0) {
        return;
    }

    append_logf("KBO player profile v20 row candidate %s player=%u active=%p active_vtbl=%p rows_off=0x%Ix rows=%p row_count=%d plausible=%d counts=%d/%d/%d/%d/%d",
        stage != NULL ? stage : "stage",
        player_id,
        (void*)active,
        (void*)*(uintptr_t*)active,
        (size_t)rows_off,
        (void*)rows,
        row_count,
        plausible_rows,
        (int)counts[0],
        (int)counts[1],
        (int)counts[2],
        (int)counts[3],
        (int)counts[4]);

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    uintptr_t expected_callback = (uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA;
    for (int r = 0; r < row_count && r < 20; r++) {
        uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
        if (row == 0 || !memory_range_readable((void*)row, 0xa0u)) {
            continue;
        }

        append_logf("KBO player profile v20 row %s player=%u rows_off=0x%Ix row=%d ptr=%p q00=%p q08=%p q10=%p q18=%p q20=%p q28=%p q30=%p q38=%p q40=%p q48=%p q50=%p q58=%p",
            stage != NULL ? stage : "stage",
            player_id,
            (size_t)rows_off,
            r,
            (void*)row,
            (void*)*(uintptr_t*)(row + 0x00u),
            (void*)*(uintptr_t*)(row + 0x08u),
            (void*)*(uintptr_t*)(row + 0x10u),
            (void*)*(uintptr_t*)(row + 0x18u),
            (void*)*(uintptr_t*)(row + 0x20u),
            (void*)*(uintptr_t*)(row + 0x28u),
            (void*)*(uintptr_t*)(row + 0x30u),
            (void*)*(uintptr_t*)(row + 0x38u),
            (void*)*(uintptr_t*)(row + 0x40u),
            (void*)*(uintptr_t*)(row + 0x48u),
            (void*)*(uintptr_t*)(row + 0x50u),
            (void*)*(uintptr_t*)(row + 0x58u));

        for (uintptr_t off = 0x00u; off <= 0x80u; off += sizeof(uintptr_t)) {
            uintptr_t vector = *(uintptr_t*)(row + off);
            if (vector == 0 || !memory_range_readable((void*)vector, sizeof(uintptr_t))) {
                continue;
            }
            int16_t count = 0;
            if (off + 0x08u + sizeof(int16_t) <= 0xa0u) {
                count = *(int16_t*)(row + off + 0x08u);
            }
            if (count <= 0 || count > 64 || !memory_range_readable((void*)vector, (size_t)count * sizeof(uintptr_t))) {
                continue;
            }

            append_logf("KBO player profile v20 vec %s player=%u rows_off=0x%Ix row=%d vec_off=0x%Ix vector=%p count=%d",
                stage != NULL ? stage : "stage",
                player_id,
                (size_t)rows_off,
                r,
                (size_t)off,
                (void*)vector,
                (int)count);

            for (int c = 0; c < count && c < 20; c++) {
                uintptr_t child = *(uintptr_t*)(vector + (size_t)c * sizeof(uintptr_t));
                int readable = child != 0 && memory_range_readable((void*)child, 0x220u);
                uintptr_t child_vtbl = readable ? *(uintptr_t*)child : 0;
                append_logf("KBO player profile v20 child %s player=%u rows_off=0x%Ix row=%d vec_off=0x%Ix col=%d child=%p readable=%d vtbl=%p menu_vtbl=%d",
                    stage != NULL ? stage : "stage",
                    player_id,
                    (size_t)rows_off,
                    r,
                    (size_t)off,
                    c,
                    (void*)child,
                    readable,
                    (void*)child_vtbl,
                    child_vtbl == expected_vtbl);
                if (!readable) {
                    continue;
                }
                uintptr_t cb40 = *(uintptr_t*)(child + 0x40u);
                uintptr_t cb48 = *(uintptr_t*)(child + 0x48u);
                uintptr_t a58 = *(uintptr_t*)(child + 0x58u);
                uintptr_t a60 = *(uintptr_t*)(child + 0x60u);
                intptr_t a68 = *(intptr_t*)(child + 0x68u);
                uintptr_t a70 = *(uintptr_t*)(child + 0x70u);
                if (child_vtbl == expected_vtbl
                        || cb40 == expected_callback
                        || cb48 == expected_callback
                        || (intptr_t)a58 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU
                        || (uint32_t)a60 == player_id
                        || a68 == KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_GLOBAL_OPEN
                        || (uint32_t)a70 == player_id) {
                    append_logf("KBO player profile v20 strong child %s player=%u rows_off=0x%Ix row=%d vec_off=0x%Ix col=%d child=%p vtbl=%p cb40=%p a58=%lld a60=%llu cb48=%p a68=%lld a70=%llu",
                        stage != NULL ? stage : "stage",
                        player_id,
                        (size_t)rows_off,
                        r,
                        (size_t)off,
                        c,
                        (void*)child,
                        (void*)child_vtbl,
                        (void*)cb40,
                        (long long)(intptr_t)a58,
                        (unsigned long long)a60,
                        (void*)cb48,
                        (long long)a68,
                        (unsigned long long)a70);
                }
            }
        }
    }
}

static void kbo_log_ootp_popup_stack_rows_v20(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t active_addr = (uintptr_t)exe + OOTP27_UI_POPUP_STACK_ACTIVE_RVA;
    if (!memory_range_readable((void*)active_addr, sizeof(uintptr_t))) {
        append_logf("KBO player profile v20 %s skipped: popup_stack_active global unreadable addr=%p",
            stage != NULL ? stage : "stage", (void*)active_addr);
        return;
    }

    uintptr_t active = *(uintptr_t*)active_addr;
    if (active == 0 || !memory_range_readable((void*)active, 0x500u)) {
        append_logf("KBO player profile v20 %s skipped: popup_stack_active invalid player=%u active=%p",
            stage != NULL ? stage : "stage", player_id, (void*)active);
        return;
    }

    append_logf("KBO player profile v20 %s active scan player=%u active=%p vtbl=%p",
        stage != NULL ? stage : "stage", player_id, (void*)active, (void*)*(uintptr_t*)active);

    for (uintptr_t off = 0x20u; off <= 0x480u; off += sizeof(uintptr_t)) {
        kbo_log_ootp_popup_stack_row_candidate_v20(exe, stage, active, off, player_id);
    }
}

static void kbo_log_ootp_popup_root_rows_v21(HMODULE exe, const char* stage, const char* label, uintptr_t root, uint32_t player_id)
{
    if (exe == NULL || root == 0) {
        return;
    }
    if (label == NULL) {
        label = "root";
    }
    if (!memory_range_readable((void*)root, 0x500u)) {
        append_logf("KBO player profile v21 %s root skipped player=%u label=%s ptr=%p readable=0",
            stage != NULL ? stage : "stage", player_id, label, (void*)root);
        return;
    }

    append_logf("KBO player profile v21 %s root scan player=%u label=%s ptr=%p vtbl=%p",
        stage != NULL ? stage : "stage", player_id, label, (void*)root, (void*)*(uintptr_t*)root);

    for (uintptr_t off = 0x20u; off <= 0x480u; off += sizeof(uintptr_t)) {
        kbo_log_ootp_popup_stack_row_candidate_v20(exe, stage, root, off, player_id);
    }
}

static uintptr_t kbo_read_ootp_global_ptr(HMODULE exe, uintptr_t rva)
{
    if (exe == NULL) {
        return 0;
    }

    uintptr_t addr = (uintptr_t)exe + rva;
    if (!memory_range_readable((void*)addr, sizeof(uintptr_t))) {
        return 0;
    }

    return *(uintptr_t*)addr;
}

static void kbo_add_ootp_popup_scan_root_v21(uintptr_t* roots, const char** labels, int* count, int max_count, uintptr_t root, const char* label)
{
    if (roots == NULL || labels == NULL || count == NULL || *count >= max_count || root == 0) {
        return;
    }

    for (int i = 0; i < *count; i++) {
        if (roots[i] == root) {
            return;
        }
    }

    roots[*count] = root;
    labels[*count] = label;
    *count += 1;
}

static void kbo_add_ootp_popup_scan_children_v21(uintptr_t* roots, const char** labels, int* count, int max_count, uintptr_t root, const char* prefix)
{
    if (root == 0 || !memory_range_readable((void*)root, 0x230u)) {
        return;
    }

    static const uintptr_t offsets[] = {
        0x40u, 0x48u, 0x50u, 0x58u, 0x60u, 0x88u, 0x90u, 0x98u,
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0x100u, 0x108u, 0x190u, 0x198u,
        0x1a0u, 0x1a8u, 0x1b0u, 0x1e0u, 0x1e8u, 0x210u, 0x218u
    };
    static const char* owner_child_labels[] = {
        "popup_owner+40", "popup_owner+48", "popup_owner+50", "popup_owner+58", "popup_owner+60", "popup_owner+88", "popup_owner+90", "popup_owner+98",
        "popup_owner+a0", "popup_owner+a8", "popup_owner+b0", "popup_owner+b8", "popup_owner+100", "popup_owner+108", "popup_owner+190", "popup_owner+198",
        "popup_owner+1a0", "popup_owner+1a8", "popup_owner+1b0", "popup_owner+1e0", "popup_owner+1e8", "popup_owner+210", "popup_owner+218"
    };
    static const char* host_child_labels[] = {
        "popup_host+40", "popup_host+48", "popup_host+50", "popup_host+58", "popup_host+60", "popup_host+88", "popup_host+90", "popup_host+98",
        "popup_host+a0", "popup_host+a8", "popup_host+b0", "popup_host+b8", "popup_host+100", "popup_host+108", "popup_host+190", "popup_host+198",
        "popup_host+1a0", "popup_host+1a8", "popup_host+1b0", "popup_host+1e0", "popup_host+1e8", "popup_host+210", "popup_host+218"
    };

    const char** child_labels = (prefix != NULL && strcmp(prefix, "popup_host") == 0) ? host_child_labels : owner_child_labels;
    for (int i = 0; i < (int)(sizeof(offsets) / sizeof(offsets[0])); i++) {
        uintptr_t child = *(uintptr_t*)(root + offsets[i]);
        if (child != 0 && memory_range_readable((void*)child, 0x80u)) {
            kbo_add_ootp_popup_scan_root_v21(roots, labels, count, max_count, child, child_labels[i]);
        }
    }
}

static void kbo_log_ootp_popup_roots_rows_v21(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t roots[64];
    const char* labels[64];
    int count = 0;

    uintptr_t popup_owner = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_OWNER_RVA);
    uintptr_t popup_host = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_HOST_RVA);
    uintptr_t popup_stack_root = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_STACK_ROOT_RVA);
    uintptr_t popup_stack_active = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_STACK_ACTIVE_RVA);
    uintptr_t popup_stack_child = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_STACK_CHILD_RVA);

    kbo_add_ootp_popup_scan_root_v21(roots, labels, &count, 64, popup_owner, "popup_owner");
    kbo_add_ootp_popup_scan_root_v21(roots, labels, &count, 64, popup_host, "popup_host");
    kbo_add_ootp_popup_scan_root_v21(roots, labels, &count, 64, popup_stack_root, "popup_stack_root");
    kbo_add_ootp_popup_scan_root_v21(roots, labels, &count, 64, popup_stack_active, "popup_stack_active");
    kbo_add_ootp_popup_scan_root_v21(roots, labels, &count, 64, popup_stack_child, "popup_stack_child");

    kbo_add_ootp_popup_scan_children_v21(roots, labels, &count, 64, popup_owner, "popup_owner");
    kbo_add_ootp_popup_scan_children_v21(roots, labels, &count, 64, popup_host, "popup_host");

    append_logf("KBO player profile v21 %s root count player=%u count=%d owner=%p host=%p stack_root=%p active=%p child=%p",
        stage != NULL ? stage : "stage",
        player_id,
        count,
        (void*)popup_owner,
        (void*)popup_host,
        (void*)popup_stack_root,
        (void*)popup_stack_active,
        (void*)popup_stack_child);

    for (int i = 0; i < count; i++) {
        kbo_log_ootp_popup_root_rows_v21(exe, stage, labels[i], roots[i], player_id);
    }
}

static int kbo_copy_ascii_preview_v22(uintptr_t ptr, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (ptr == 0 || !memory_range_readable((void*)ptr, 1)) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < 48; len++) {
        if (!memory_range_readable((void*)(ptr + len), 1)) {
            break;
        }
        unsigned char ch = *(unsigned char*)(ptr + len);
        if (ch == 0) {
            break;
        }
        if (ch < 0x20 || ch > 0x7e) {
            if (len < 3) {
                out[0] = '\0';
                return 0;
            }
            break;
        }
        out[len] = (char)ch;
    }
    out[len] = '\0';
    return len >= 3;
}

static void kbo_log_ootp_menu_child_detail_v22(HMODULE exe, const char* stage, const char* label, uintptr_t child, uint32_t player_id)
{
    if (exe == NULL || child == 0 || !memory_range_readable((void*)child, 0x240u)) {
        return;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    uintptr_t vtbl = *(uintptr_t*)child;
    append_logf("KBO player profile v22 detail %s player=%u label=%s child=%p vtbl=%p menu_vtbl=%d",
        stage != NULL ? stage : "stage",
        player_id,
        label != NULL ? label : "child",
        (void*)child,
        (void*)vtbl,
        vtbl == expected_vtbl);

    for (uintptr_t off = 0x00u; off <= 0x220u; off += 0x08u) {
        uintptr_t value = *(uintptr_t*)(child + off);
        char preview[64];
        int has_preview = kbo_copy_ascii_preview_v22(value, preview, sizeof(preview));
        append_logf("KBO player profile v22 field %s player=%u label=%s child=%p off=0x%Ix value=%p int=%lld str=%s%s%s",
            stage != NULL ? stage : "stage",
            player_id,
            label != NULL ? label : "child",
            (void*)child,
            (size_t)off,
            (void*)value,
            (long long)(intptr_t)value,
            has_preview ? "\"" : "",
            has_preview ? preview : "",
            has_preview ? "\"" : "");
    }
}

static void kbo_log_ootp_popup_target_rows_v22(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t owner = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_OWNER_RVA);
    if (owner == 0 || !memory_range_readable((void*)owner, 0x120u)) {
        append_logf("KBO player profile v22 %s skipped: owner invalid player=%u owner=%p",
            stage != NULL ? stage : "stage", player_id, (void*)owner);
        return;
    }

    uintptr_t root = *(uintptr_t*)(owner + 0x90u);
    if (root == 0 || !memory_range_readable((void*)root, 0x120u)) {
        append_logf("KBO player profile v22 %s skipped: owner+90 invalid player=%u owner=%p root=%p",
            stage != NULL ? stage : "stage", player_id, (void*)owner, (void*)root);
        return;
    }

    uintptr_t rows = *(uintptr_t*)(root + 0xe8u);
    int16_t row_count = *(int16_t*)(root + 0xf0u);
    append_logf("KBO player profile v22 %s target root player=%u owner=%p root=%p root_vtbl=%p rows=%p row_count=%d",
        stage != NULL ? stage : "stage",
        player_id,
        (void*)owner,
        (void*)root,
        (void*)*(uintptr_t*)root,
        (void*)rows,
        (int)row_count);
    if (rows == 0 || row_count <= 0 || row_count > 64 || !memory_range_readable((void*)rows, (size_t)row_count * sizeof(uintptr_t))) {
        return;
    }

    uintptr_t expected_vtbl = (uintptr_t)exe + OOTP27_UI_MENU_ITEM_VTABLE_RVA;
    for (int r = 0; r < row_count && r < 24; r++) {
        uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
        if (row == 0 || !memory_range_readable((void*)row, 0x80u)) {
            continue;
        }

        uintptr_t children = *(uintptr_t*)(row + 0x08u);
        int16_t child_count = *(int16_t*)(row + 0x10u);
        append_logf("KBO player profile v22 row %s player=%u row=%d ptr=%p children=%p child_count=%d q00=%p q18=%p q48=%p q50=%p",
            stage != NULL ? stage : "stage",
            player_id,
            r,
            (void*)row,
            (void*)children,
            (int)child_count,
            (void*)*(uintptr_t*)(row + 0x00u),
            (void*)*(uintptr_t*)(row + 0x18u),
            (void*)*(uintptr_t*)(row + 0x48u),
            (void*)*(uintptr_t*)(row + 0x50u));
        if (children == 0 || child_count <= 0 || child_count > 32
                || !memory_range_readable((void*)children, (size_t)child_count * sizeof(uintptr_t))) {
            continue;
        }

        for (int c = 0; c < child_count; c++) {
            uintptr_t child = *(uintptr_t*)(children + (size_t)c * sizeof(uintptr_t));
            if (child == 0 || !memory_range_readable((void*)child, 0x240u)) {
                continue;
            }

            uintptr_t child_vtbl = *(uintptr_t*)child;
            if (child_vtbl == expected_vtbl) {
                char label[64];
                snprintf(label, sizeof(label), "owner90-e8-r%d-c%d", r, c);
                kbo_log_ootp_menu_child_detail_v22(exe, stage, label, child, player_id);
            }
        }
    }
}

static void kbo_log_ootp_popup_child_summary_v23(HMODULE exe, const char* stage, int row_index, int col_index, uintptr_t child, uint32_t player_id)
{
    if (exe == NULL || child == 0 || !memory_range_readable((void*)child, 0x120u)) {
        return;
    }

    char text_f0[64];
    char text_e8[64];
    char text_1e0[64];
    uintptr_t p_f0 = memory_range_readable((void*)(child + 0xf0u), sizeof(uintptr_t)) ? *(uintptr_t*)(child + 0xf0u) : 0;
    uintptr_t p_e8 = memory_range_readable((void*)(child + 0xe8u), sizeof(uintptr_t)) ? *(uintptr_t*)(child + 0xe8u) : 0;
    uintptr_t p_1e0 = memory_range_readable((void*)(child + 0x1e0u), sizeof(uintptr_t)) ? *(uintptr_t*)(child + 0x1e0u) : 0;
    int has_f0 = kbo_copy_ascii_preview_v22(p_f0, text_f0, sizeof(text_f0));
    int has_e8 = kbo_copy_ascii_preview_v22(p_e8, text_e8, sizeof(text_e8));
    int has_1e0 = kbo_copy_ascii_preview_v22(p_1e0, text_1e0, sizeof(text_1e0));

    uintptr_t cb40 = memory_range_readable((void*)(child + 0x40u), sizeof(uintptr_t)) ? *(uintptr_t*)(child + 0x40u) : 0;
    uintptr_t cb48 = memory_range_readable((void*)(child + 0x48u), sizeof(uintptr_t)) ? *(uintptr_t*)(child + 0x48u) : 0;
    uintptr_t a58 = memory_range_readable((void*)(child + 0x58u), sizeof(uintptr_t)) ? *(uintptr_t*)(child + 0x58u) : 0;
    uintptr_t a60 = memory_range_readable((void*)(child + 0x60u), sizeof(uintptr_t)) ? *(uintptr_t*)(child + 0x60u) : 0;
    intptr_t a68 = memory_range_readable((void*)(child + 0x68u), sizeof(intptr_t)) ? *(intptr_t*)(child + 0x68u) : 0;
    uintptr_t a70 = memory_range_readable((void*)(child + 0x70u), sizeof(uintptr_t)) ? *(uintptr_t*)(child + 0x70u) : 0;

    append_logf("KBO player profile v23 child %s player=%u row=%d col=%d child=%p vtbl=%p cb40=%p cb48=%p a58=%lld a60=%llu a68=%lld a70=%llu p90=%p p98=%p text_e8=%s%s%s text_f0=%s%s%s text_1e0=%s%s%s",
        stage != NULL ? stage : "stage",
        player_id,
        row_index,
        col_index,
        (void*)child,
        (void*)*(uintptr_t*)child,
        (void*)cb40,
        (void*)cb48,
        (long long)(intptr_t)a58,
        (unsigned long long)a60,
        (long long)a68,
        (unsigned long long)a70,
        memory_range_readable((void*)(child + 0x90u), sizeof(uintptr_t)) ? (void*)*(uintptr_t*)(child + 0x90u) : NULL,
        memory_range_readable((void*)(child + 0x98u), sizeof(uintptr_t)) ? (void*)*(uintptr_t*)(child + 0x98u) : NULL,
        has_e8 ? "\"" : "",
        has_e8 ? text_e8 : "",
        has_e8 ? "\"" : "",
        has_f0 ? "\"" : "",
        has_f0 ? text_f0 : "",
        has_f0 ? "\"" : "",
        has_1e0 ? "\"" : "",
        has_1e0 ? text_1e0 : "",
        has_1e0 ? "\"" : "");
}

static void kbo_log_ootp_popup_target_rows_v23(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t owner = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_OWNER_RVA);
    uintptr_t roots[3] = { 0, 0, 0 };
    if (owner != 0 && memory_range_readable((void*)owner, 0x120u)) {
        roots[0] = owner;
        roots[1] = *(uintptr_t*)(owner + 0x90u);
        roots[2] = *(uintptr_t*)(owner + 0x98u);
    }

    for (int root_i = 0; root_i < 3; root_i++) {
        uintptr_t root = roots[root_i];
        if (root == 0 || !memory_range_readable((void*)root, 0x500u)) {
            continue;
        }

        for (uintptr_t rows_off = 0x20u; rows_off <= 0x480u; rows_off += 0x08u) {
            uintptr_t rows = *(uintptr_t*)(root + rows_off);
            int16_t row_count = 0;
            if (rows_off + 0x08u + sizeof(int16_t) <= 0x500u) {
                row_count = *(int16_t*)(root + rows_off + 0x08u);
            }
            if (rows == 0 || row_count <= 0 || row_count > 32
                    || !memory_range_readable((void*)rows, (size_t)row_count * sizeof(uintptr_t))) {
                continue;
            }

            int plausible = 0;
            for (int r = 0; r < row_count && r < 8; r++) {
                uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
                if (row != 0 && memory_range_readable((void*)row, 0x20u)) {
                    uintptr_t children = *(uintptr_t*)(row + 0x08u);
                    int16_t child_count = *(int16_t*)(row + 0x10u);
                    if (children != 0 && child_count > 0 && child_count <= 16
                            && memory_range_readable((void*)children, (size_t)child_count * sizeof(uintptr_t))) {
                        plausible++;
                    }
                }
            }
            if (plausible == 0) {
                continue;
            }

            append_logf("KBO player profile v23 root %s player=%u root_i=%d root=%p vtbl=%p rows_off=0x%Ix rows=%p row_count=%d plausible=%d",
                stage != NULL ? stage : "stage",
                player_id,
                root_i,
                (void*)root,
                (void*)*(uintptr_t*)root,
                (size_t)rows_off,
                (void*)rows,
                (int)row_count,
                plausible);

            for (int r = 0; r < row_count && r < 24; r++) {
                uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
                if (row == 0 || !memory_range_readable((void*)row, 0x20u)) {
                    continue;
                }
                uintptr_t children = *(uintptr_t*)(row + 0x08u);
                int16_t child_count = *(int16_t*)(row + 0x10u);
                if (children == 0 || child_count <= 0 || child_count > 16
                        || !memory_range_readable((void*)children, (size_t)child_count * sizeof(uintptr_t))) {
                    continue;
                }

                for (int c = 0; c < child_count; c++) {
                    uintptr_t child = *(uintptr_t*)(children + (size_t)c * sizeof(uintptr_t));
                    if (child != 0 && memory_range_readable((void*)child, 0x220u)) {
                        kbo_log_ootp_popup_child_summary_v23(exe, stage, r, c, child, player_id);
                    }
                }
            }
        }
    }
}

static void kbo_qword_ascii_preview_v24(uintptr_t value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    size_t n = 0;
    for (int i = 0; i < 8 && n + 1 < out_size; i++) {
        unsigned char ch = (unsigned char)((value >> (i * 8)) & 0xffu);
        if (ch >= 0x20 && ch <= 0x7e) {
            out[n++] = (char)ch;
        } else if (ch == 0) {
            continue;
        } else {
            out[n++] = '.';
        }
    }
    out[n] = '\0';
}

static void kbo_qword_utf16_preview_v24(uintptr_t value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    size_t n = 0;
    for (int i = 0; i < 4 && n + 1 < out_size; i++) {
        unsigned char lo = (unsigned char)((value >> (i * 16)) & 0xffu);
        unsigned char hi = (unsigned char)((value >> (i * 16 + 8)) & 0xffu);
        if (hi == 0 && lo >= 0x20 && lo <= 0x7e) {
            out[n++] = (char)lo;
        }
    }
    out[n] = '\0';
}

static void kbo_log_ootp_object_words_v24(const char* stage, const char* label, uintptr_t root, uint32_t player_id)
{
    if (root == 0 || !memory_range_readable((void*)root, 0x280u)) {
        append_logf("KBO player profile v24 words skipped %s player=%u label=%s root=%p readable=0",
            stage != NULL ? stage : "stage", player_id, label != NULL ? label : "root", (void*)root);
        return;
    }

    append_logf("KBO player profile v24 words begin %s player=%u label=%s root=%p vtbl=%p",
        stage != NULL ? stage : "stage", player_id, label != NULL ? label : "root", (void*)root, (void*)*(uintptr_t*)root);

    for (uintptr_t off = 0; off <= 0x260u; off += 0x08u) {
        uintptr_t value = *(uintptr_t*)(root + off);
        char bytes[16];
        char wide[8];
        char ptr_text[64];
        kbo_qword_ascii_preview_v24(value, bytes, sizeof(bytes));
        kbo_qword_utf16_preview_v24(value, wide, sizeof(wide));
        int has_ptr_text = kbo_copy_ascii_preview_v22(value, ptr_text, sizeof(ptr_text));
        if ((bytes[0] != '\0' && strlen(bytes) >= 3)
                || (wide[0] != '\0' && strlen(wide) >= 2)
                || has_ptr_text
                || (value >= (uintptr_t)0x10000u && memory_range_readable((void*)value, 0x20u))) {
            append_logf("KBO player profile v24 word %s player=%u label=%s root=%p off=0x%Ix value=%p bytes=\"%s\" wide=\"%s\" ptr_text=%s%s%s",
                stage != NULL ? stage : "stage",
                player_id,
                label != NULL ? label : "root",
                (void*)root,
                (size_t)off,
                (void*)value,
                bytes,
                wide,
                has_ptr_text ? "\"" : "",
                has_ptr_text ? ptr_text : "",
                has_ptr_text ? "\"" : "");
        }
    }
}

static void kbo_log_ootp_popup_rows_for_root_v24(HMODULE exe, const char* stage, const char* label, uintptr_t root, uint32_t player_id)
{
    if (exe == NULL || root == 0 || !memory_range_readable((void*)root, 0x500u)) {
        return;
    }

    for (uintptr_t rows_off = 0x20u; rows_off <= 0x480u; rows_off += 0x08u) {
        uintptr_t rows = *(uintptr_t*)(root + rows_off);
        int16_t counts[5] = {0};
        int row_count = 0;
        for (int i = 0; i < 5; i++) {
            uintptr_t count_off = rows_off + 0x08u + (uintptr_t)i * 0x08u;
            if (count_off + sizeof(int16_t) <= 0x500u) {
                counts[i] = *(int16_t*)(root + count_off);
                if (counts[i] > row_count && counts[i] <= 64) {
                    row_count = counts[i];
                }
            }
        }
        if (rows == 0 || row_count <= 0 || row_count > 64
                || !memory_range_readable((void*)rows, (size_t)row_count * sizeof(uintptr_t))) {
            continue;
        }

        int plausible = 0;
        for (int r = 0; r < row_count && r < 12; r++) {
            uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
            if (row != 0 && memory_range_readable((void*)row, 0x20u)) {
                uintptr_t children = *(uintptr_t*)(row + 0x08u);
                int16_t child_count = *(int16_t*)(row + 0x10u);
                if (children != 0 && child_count > 0 && child_count <= 64
                        && memory_range_readable((void*)children, (size_t)child_count * sizeof(uintptr_t))) {
                    plausible++;
                }
            }
        }
        if (plausible == 0) {
            continue;
        }

        append_logf("KBO player profile v24 root %s player=%u label=%s root=%p vtbl=%p rows_off=0x%Ix rows=%p row_count=%d plausible=%d counts=%d/%d/%d/%d/%d",
            stage != NULL ? stage : "stage",
            player_id,
            label != NULL ? label : "root",
            (void*)root,
            (void*)*(uintptr_t*)root,
            (size_t)rows_off,
            (void*)rows,
            row_count,
            plausible,
            (int)counts[0],
            (int)counts[1],
            (int)counts[2],
            (int)counts[3],
            (int)counts[4]);

        for (int r = 0; r < row_count && r < 24; r++) {
            uintptr_t row = *(uintptr_t*)(rows + (size_t)r * sizeof(uintptr_t));
            if (row == 0 || !memory_range_readable((void*)row, 0x20u)) {
                continue;
            }
            uintptr_t children = *(uintptr_t*)(row + 0x08u);
            int16_t child_count = *(int16_t*)(row + 0x10u);
            if (children == 0 || child_count <= 0 || child_count > 64
                    || !memory_range_readable((void*)children, (size_t)child_count * sizeof(uintptr_t))) {
                continue;
            }
            for (int c = 0; c < child_count && c < 32; c++) {
                uintptr_t child = *(uintptr_t*)(children + (size_t)c * sizeof(uintptr_t));
                if (child != 0 && memory_range_readable((void*)child, 0x220u)) {
                    kbo_log_ootp_popup_child_summary_v23(exe, stage, r, c, child, player_id);
                }
            }
        }
    }
}

static void kbo_log_ootp_popup_targets_v24(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t owner = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_OWNER_RVA);
    uintptr_t host = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_HOST_RVA);
    uintptr_t stack_root = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_STACK_ROOT_RVA);
    uintptr_t stack_active = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_STACK_ACTIVE_RVA);
    uintptr_t stack_child = kbo_read_ootp_global_ptr(exe, OOTP27_UI_POPUP_STACK_CHILD_RVA);

    uintptr_t roots[12] = { owner, host, stack_root, stack_active, stack_child, 0, 0, 0, 0, 0, 0, 0 };
    const char* labels[12] = { "popup_owner", "popup_host", "stack_root", "stack_active", "stack_child", "owner+90", "owner+98", "host+1a8", "host+210", "active+1a8", "active+210", "child+1a8" };
    if (owner != 0 && memory_range_readable((void*)owner, 0x120u)) {
        roots[5] = *(uintptr_t*)(owner + 0x90u);
        roots[6] = *(uintptr_t*)(owner + 0x98u);
    }
    if (host != 0 && memory_range_readable((void*)host, 0x220u)) {
        roots[7] = *(uintptr_t*)(host + 0x1a8u);
        roots[8] = *(uintptr_t*)(host + 0x210u);
    }
    if (stack_active != 0 && memory_range_readable((void*)stack_active, 0x220u)) {
        roots[9] = *(uintptr_t*)(stack_active + 0x1a8u);
        roots[10] = *(uintptr_t*)(stack_active + 0x210u);
    }
    if (stack_child != 0 && memory_range_readable((void*)stack_child, 0x220u)) {
        roots[11] = *(uintptr_t*)(stack_child + 0x1a8u);
    }

    append_logf("KBO player profile v24 roots %s player=%u owner=%p host=%p stack_root=%p stack_active=%p stack_child=%p",
        stage != NULL ? stage : "stage",
        player_id,
        (void*)owner,
        (void*)host,
        (void*)stack_root,
        (void*)stack_active,
        (void*)stack_child);

    for (int i = 0; i < 12; i++) {
        if (roots[i] == 0) {
            continue;
        }
        kbo_log_ootp_object_words_v24(stage, labels[i], roots[i], player_id);
        kbo_log_ootp_popup_rows_for_root_v24(exe, stage, labels[i], roots[i], player_id);
    }
}

static int kbo_mem_contains_ascii_v25(const unsigned char* data, size_t data_len, const char* needle)
{
    size_t needle_len = needle != NULL ? strlen(needle) : 0u;
    if (data == NULL || needle_len == 0u || data_len < needle_len) {
        return 0;
    }
    for (size_t i = 0; i + needle_len <= data_len; i++) {
        if (memcmp(data + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static void kbo_copy_memory_ascii_window_v25(uintptr_t address, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (address < 0x10000u || !memory_range_readable((void*)address, 0x60u)) {
        return;
    }

    const unsigned char* p = (const unsigned char*)address;
    size_t n = 0;
    for (size_t i = 0; i < 0x60u && n + 1 < out_size; i++) {
        unsigned char ch = p[i];
        if (ch == 0) {
            break;
        }
        out[n++] = (ch >= 0x20 && ch <= 0x7e) ? (char)ch : '.';
    }
    out[n] = '\0';
}

static void kbo_log_string_pointer_refs_v25(uint32_t player_id, uintptr_t text_addr)
{
    if (text_addr == 0) {
        return;
    }

    uintptr_t cursor = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    int refs_logged = 0;
    SIZE_T scanned = 0;

    while (VirtualQuery((void*)cursor, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= cursor) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && protect_allows_read(mbi.Protect)
                && (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED || mbi.Type == MEM_IMAGE)
                && mbi.RegionSize <= (64u * 1024u * 1024u)) {
            uintptr_t scan_start = base;
            uintptr_t scan_end = end;
            if (scan_end > scan_start + (64u * 1024u * 1024u)) {
                scan_end = scan_start + (64u * 1024u * 1024u);
            }

            for (uintptr_t p = scan_start; p + sizeof(uintptr_t) <= scan_end; p += sizeof(uintptr_t)) {
                if (*(uintptr_t*)p != text_addr) {
                    continue;
                }

                uintptr_t object_base = 0;
                uintptr_t object_vtbl = 0;
                for (uintptr_t off = 0; off <= 0x220u; off += 0x08u) {
                    if (p < off + 0x10000u) {
                        continue;
                    }
                    uintptr_t candidate = p - off;
                    if (memory_range_readable((void*)candidate, 0x230u)) {
                        uintptr_t vtbl = *(uintptr_t*)candidate;
                        if (vtbl != 0 && memory_range_readable((void*)vtbl, sizeof(uintptr_t))) {
                            object_base = candidate;
                            object_vtbl = vtbl;
                            break;
                        }
                    }
                }

                uintptr_t q_m20 = 0, q_m18 = 0, q_m10 = 0, q_m08 = 0, q_p08 = 0, q_p10 = 0, q_p18 = 0, q_p20 = 0;
                if (p >= 0x20u && memory_range_readable((void*)(p - 0x20u), 0x48u)) {
                    q_m20 = *(uintptr_t*)(p - 0x20u);
                    q_m18 = *(uintptr_t*)(p - 0x18u);
                    q_m10 = *(uintptr_t*)(p - 0x10u);
                    q_m08 = *(uintptr_t*)(p - 0x08u);
                    q_p08 = *(uintptr_t*)(p + 0x08u);
                    q_p10 = *(uintptr_t*)(p + 0x10u);
                    q_p18 = *(uintptr_t*)(p + 0x18u);
                    q_p20 = *(uintptr_t*)(p + 0x20u);
                }

                append_logf("KBO player profile v25 text ref player=%u text=%p ref=%p object=%p vtbl=%p nearby=%p/%p/%p/%p/%p/%p/%p/%p",
                    player_id,
                    (void*)text_addr,
                    (void*)p,
                    (void*)object_base,
                    (void*)object_vtbl,
                    (void*)q_m20,
                    (void*)q_m18,
                    (void*)q_m10,
                    (void*)q_m08,
                    (void*)q_p08,
                    (void*)q_p10,
                    (void*)q_p18,
                    (void*)q_p20);
                refs_logged++;
                if (refs_logged >= 10) {
                    append_logf("KBO player profile v25 text refs capped player=%u text=%p refs=%d",
                        player_id, (void*)text_addr, refs_logged);
                    return;
                }
            }
            scanned += scan_end - scan_start;
            if (scanned > (512u * 1024u * 1024u)) {
                break;
            }
        }

        cursor = end;
    }

    append_logf("KBO player profile v25 text refs done player=%u text=%p refs=%d scanned_mb=%llu",
        player_id, (void*)text_addr, refs_logged, (unsigned long long)(scanned / (1024u * 1024u)));
}

static void kbo_log_player_profile_menu_text_scan_v25(uint32_t player_id)
{
    static const char* needles[] = {
        "Open Player Profile",
        "Player Profile",
        "Open Profile",
        "Profile"
    };

    uintptr_t cursor = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    int hits_logged = 0;
    SIZE_T scanned = 0;
    uintptr_t first_strong_hit = 0;

    while (VirtualQuery((void*)cursor, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= cursor) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && protect_allows_read(mbi.Protect)
                && (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED || mbi.Type == MEM_IMAGE)
                && mbi.RegionSize <= (64u * 1024u * 1024u)) {
            uintptr_t scan_start = base;
            uintptr_t scan_end = end;
            if (scan_end > scan_start + (64u * 1024u * 1024u)) {
                scan_end = scan_start + (64u * 1024u * 1024u);
            }

            const unsigned char* data = (const unsigned char*)scan_start;
            size_t len = (size_t)(scan_end - scan_start);
            for (size_t i = 0; i < len && hits_logged < 24; i++) {
                for (int n = 0; n < (int)(sizeof(needles) / sizeof(needles[0])); n++) {
                    const char* needle = needles[n];
                    size_t needle_len = strlen(needle);
                    if (i + needle_len > len || memcmp(data + i, needle, needle_len) != 0) {
                        continue;
                    }

                    uintptr_t hit = scan_start + i;
                    char preview[128];
                    kbo_copy_memory_ascii_window_v25(hit, preview, sizeof(preview));
                    append_logf("KBO player profile v25 text hit player=%u needle=\"%s\" addr=%p region=%p size=%llu type=0x%lx protect=0x%lx text=\"%s\"",
                        player_id,
                        needle,
                        (void*)hit,
                        (void*)base,
                        (unsigned long long)mbi.RegionSize,
                        (unsigned long)mbi.Type,
                        (unsigned long)mbi.Protect,
                        preview);
                    if (first_strong_hit == 0 && (strcmp(needle, "Open Player Profile") == 0 || strcmp(needle, "Player Profile") == 0)) {
                        first_strong_hit = hit;
                    }
                    hits_logged++;
                    i += needle_len > 0 ? needle_len - 1u : 0u;
                    break;
                }
            }
            scanned += scan_end - scan_start;
            if (scanned > (512u * 1024u * 1024u) || hits_logged >= 24) {
                break;
            }
        }

        cursor = end;
    }

    append_logf("KBO player profile v25 text scan done player=%u hits=%d strong=%p scanned_mb=%llu",
        player_id, hits_logged, (void*)first_strong_hit, (unsigned long long)(scanned / (1024u * 1024u)));
    if (first_strong_hit != 0) {
        kbo_log_string_pointer_refs_v25(player_id, first_strong_hit);
    }
}

static void kbo_log_ootp_ui_overlay_globals(HMODULE exe, const char* stage, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }
    if (stage == NULL) {
        stage = "overlay";
    }

    static const struct {
        const char* name;
        uintptr_t rva;
    } slots[] = {
        { "input_state", OOTP27_UI_INPUT_STATE_RVA },
        { "mouse_root", OOTP27_UI_MOUSE_ROOT_RVA },
        { "popup_owner", OOTP27_UI_POPUP_OWNER_RVA },
        { "active_widget", OOTP27_UI_ACTIVE_WIDGET_RVA },
    };

    for (int i = 0; i < (int)(sizeof(slots) / sizeof(slots[0])); i++) {
        uintptr_t addr = (uintptr_t)exe + slots[i].rva;
        if (!memory_range_readable((void*)addr, sizeof(uintptr_t))) {
            append_logf("KBO player profile overlay %s global %s unreadable addr=%p",
                stage, slots[i].name, (void*)addr);
            continue;
        }

        uintptr_t ptr = *(uintptr_t*)addr;
        uintptr_t vtbl = 0;
        int readable = ptr != 0 && memory_range_readable((void*)ptr, 0x220u);
        if (readable) {
            vtbl = *(uintptr_t*)ptr;
        }
        append_logf("KBO player profile overlay %s global %s addr=%p ptr=%p readable=%d vtbl=%p",
            stage, slots[i].name, (void*)addr, (void*)ptr, readable, (void*)vtbl);
        if (readable) {
            kbo_log_ootp_widget_candidate(slots[i].name, (void*)ptr, player_id);
        }
    }

    uintptr_t tick_addr = (uintptr_t)exe + OOTP27_UI_ACTIVE_WIDGET_TICK_RVA;
    if (memory_range_readable((void*)tick_addr, sizeof(uint32_t))) {
        append_logf("KBO player profile overlay %s active_widget_tick=%u addr=%p",
            stage, *(uint32_t*)tick_addr, (void*)tick_addr);
    }
}

static int kbo_try_player_profile_global_open_v10(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v10_last_ms < 900u) {
        append_logf("KBO player profile global open v10 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v10_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v10_last_ms = now;

    kbo_log_ootp_ui_overlay_globals(exe, "before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_ui_overlay_globals(exe, "after_popup", player_id);
    append_logf("KBO player profile global open v10: popup probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v11(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v11_last_ms < 900u) {
        append_logf("KBO player profile global open v11 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v11_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v11_last_ms = now;

    kbo_log_ootp_ui_overlay_globals(exe, "before_popup", player_id);
    kbo_log_ootp_popup_owner_children(exe, "before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_ui_overlay_globals(exe, "after_popup", player_id);
    kbo_log_ootp_popup_owner_children(exe, "after_popup", player_id);
    append_logf("KBO player profile global open v11: popup child probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v12(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v12_last_ms < 900u) {
        append_logf("KBO player profile global open v12 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v12_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v12_last_ms = now;

    kbo_log_ootp_ui_overlay_globals(exe, "v12_before_popup", player_id);
    kbo_log_ootp_popup_owner_focused_children(exe, "v12_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_ui_overlay_globals(exe, "v12_after_popup", player_id);
    kbo_log_ootp_popup_owner_focused_children(exe, "v12_after_popup", player_id);
    append_logf("KBO player profile global open v12: focused child probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v13(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v13_last_ms < 900u) {
        append_logf("KBO player profile global open v13 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v13_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v13_last_ms = now;

    kbo_log_ootp_popup_owner_rows(exe, "v13_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_owner_rows(exe, "v13_after_popup", player_id);
    append_logf("KBO player profile global open v13: row child probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v14(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v14_last_ms < 900u) {
        append_logf("KBO player profile global open v14 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v14_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v14_last_ms = now;

    kbo_log_ootp_popup_owner_row_candidates(exe, "v14_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_owner_row_candidates(exe, "v14_after_popup", player_id);
    append_logf("KBO player profile global open v14: row candidate probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v15(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v15_last_ms < 900u) {
        append_logf("KBO player profile global open v15 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v15_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v15_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v15_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v15_after_popup", player_id);
    append_logf("KBO player profile global open v15: popup globals probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v16(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v16_last_ms < 900u) {
        append_logf("KBO player profile global open v16 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v16_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v16_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v16_before_popup", player_id);
    kbo_log_ootp_popup_host_deep_v16(exe, "v16_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v16_after_popup", player_id);
    kbo_log_ootp_popup_host_deep_v16(exe, "v16_after_popup", player_id);
    append_logf("KBO player profile global open v16: popup host deep probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v17(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v17_last_ms < 900u) {
        append_logf("KBO player profile global open v17 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v17_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v17_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v17_before_popup", player_id);
    kbo_log_ootp_popup_host_menu_probe_v17(exe, "v17_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v17_after_popup", player_id);
    kbo_log_ootp_popup_host_menu_probe_v17(exe, "v17_after_popup", player_id);
    append_logf("KBO player profile global open v17: popup menu probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v18(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v18_last_ms < 900u) {
        append_logf("KBO player profile global open v18 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v18_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v18_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v18_before_popup", player_id);
    kbo_log_ootp_popup_stack_probe_v18(exe, "v18_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v18_after_popup", player_id);
    kbo_log_ootp_popup_stack_probe_v18(exe, "v18_after_popup", player_id);
    append_logf("KBO player profile global open v18: popup stack probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v19(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v19_last_ms < 900u) {
        append_logf("KBO player profile global open v19 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v19_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v19_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v19_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v19_after_popup", player_id);
    kbo_log_ootp_popup_stack_rows_v19(exe, "v19_after_popup", player_id);
    append_logf("KBO player profile global open v19: popup row probe done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v20(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v20_last_ms < 900u) {
        append_logf("KBO player profile global open v20 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v20_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v20_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v20_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v20_after_popup", player_id);
    kbo_log_ootp_popup_stack_rows_v20(exe, "v20_after_popup", player_id);
    append_logf("KBO player profile global open v20: popup active row scan done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v21(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v21_last_ms < 900u) {
        append_logf("KBO player profile global open v21 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v21_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v21_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v21_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v21_after_popup", player_id);
    kbo_log_ootp_popup_roots_rows_v21(exe, "v21_after_popup", player_id);
    append_logf("KBO player profile global open v21: popup root row scan done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v22(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v22_last_ms < 900u) {
        append_logf("KBO player profile global open v22 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v22_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v22_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v22_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v22_after_popup", player_id);
    kbo_log_ootp_popup_target_rows_v22(exe, "v22_after_popup", player_id);
    append_logf("KBO player profile global open v22: popup target detail scan done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v23(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v23_last_ms < 900u) {
        append_logf("KBO player profile global open v23 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v23_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v23_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v23_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v23_after_popup", player_id);
    kbo_log_ootp_popup_target_rows_v23(exe, "v23_after_popup", player_id);
    append_logf("KBO player profile global open v23: popup all child text scan done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v24(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v24_last_ms < 900u) {
        append_logf("KBO player profile global open v24 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v24_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v24_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v24_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v24_after_popup", player_id);
    kbo_log_ootp_popup_targets_v24(exe, "v24_after_popup", player_id);
    append_logf("KBO player profile global open v24: popup stack active text scan done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

static int kbo_try_player_profile_global_open_v25(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_global_open_v25_last_ms < 900u) {
        append_logf("KBO player profile global open v25 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_global_open_v25_last_ms));
        return 0;
    }
    g_kbo_player_profile_global_open_v25_last_ms = now;

    kbo_log_ootp_popup_globals_v15(exe, "v25_before_popup", player_id);
    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    kbo_log_ootp_popup_globals_v15(exe, "v25_after_popup", player_id);
    kbo_log_player_profile_menu_text_scan_v25(player_id);
    append_logf("KBO player profile global open v25: menu text memory scan done player=%u popup_opened=%d",
        player_id, popup_opened);
    return popup_opened;
}

typedef struct KboPlayerProfileOcrClickJob {
    HWND hwnd;
    RECT capture_rect;
    uint32_t player_id;
    int popup_opened;
    unsigned char* before_pixels;
    int before_width;
    int before_height;
    RECT crop_rect;
} KboPlayerProfileOcrClickJob;

typedef struct KboPlayerProfileOcrWord {
    int left;
    int top;
    int right;
    int bottom;
    int line_key;
    double conf;
    char raw[96];
    char norm[48];
} KboPlayerProfileOcrWord;

static void kbo_profile_ocr_normalize_word(const char* text, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (text == NULL) {
        return;
    }

    size_t write = 0;
    for (size_t i = 0; text[i] != '\0' && write + 1 < out_size; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch >= 'A' && ch <= 'Z') {
            out[write++] = (char)(ch - 'A' + 'a');
        } else if (ch >= 'a' && ch <= 'z') {
            out[write++] = (char)ch;
        } else if (ch == '0') {
            out[write++] = 'o';
        } else if (ch == '1' || ch == '|' || ch == '!') {
            out[write++] = 'l';
        }
    }
    out[write] = '\0';
}

static int kbo_profile_ocr_edit_distance_at_most_one(const char* a, const char* b)
{
    size_t alen = a != NULL ? strlen(a) : 0u;
    size_t blen = b != NULL ? strlen(b) : 0u;
    if (alen == 0u || blen == 0u) {
        return 0;
    }
    if (alen > blen + 1u || blen > alen + 1u) {
        return 0;
    }

    size_t i = 0;
    size_t j = 0;
    int edits = 0;
    while (i < alen && j < blen) {
        if (a[i] == b[j]) {
            i++;
            j++;
            continue;
        }
        edits++;
        if (edits > 1) {
            return 0;
        }
        if (alen == blen) {
            i++;
            j++;
        } else if (alen > blen) {
            i++;
        } else {
            j++;
        }
    }
    if (i < alen || j < blen) {
        edits++;
    }
    return edits <= 1;
}

static int kbo_profile_ocr_word_matches(const char* norm, const char* target)
{
    if (norm == NULL || target == NULL || norm[0] == '\0') {
        return 0;
    }
    return strcmp(norm, target) == 0 || kbo_profile_ocr_edit_distance_at_most_one(norm, target);
}

static void kbo_profile_ocr_quote_arg(const char* path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (path == NULL) {
        return;
    }

    size_t write = 0;
    if (write + 1 < out_size) {
        out[write++] = '"';
    }
    for (size_t i = 0; path[i] != '\0' && write + 2 < out_size; i++) {
        if (path[i] == '"') {
            out[write++] = '\\';
        }
        out[write++] = path[i];
    }
    if (write + 1 < out_size) {
        out[write++] = '"';
    }
    out[write] = '\0';
}

static int kbo_profile_ocr_find_tesseract(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got > 0 && got < sizeof(local_app_data)) {
        char override_path[MAX_PATH] = {0};
        snprintf(override_path, sizeof(override_path), "%s\\OOTP-KBO\\tesseract_path.txt", local_app_data);
        HANDLE file = CreateFileA(override_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            char buffer[MAX_PATH] = {0};
            DWORD read = 0;
            ReadFile(file, buffer, sizeof(buffer) - 1, &read, NULL);
            CloseHandle(file);
            while (read > 0 && (buffer[read - 1] == '\r' || buffer[read - 1] == '\n' || buffer[read - 1] == ' ' || buffer[read - 1] == '\t')) {
                buffer[--read] = '\0';
            }
            if (buffer[0] != '\0' && GetFileAttributesA(buffer) != INVALID_FILE_ATTRIBUTES) {
                snprintf(out, out_size, "%s", buffer);
                return 1;
            }
        }

        char bundled_path[MAX_PATH] = {0};
        snprintf(bundled_path, sizeof(bundled_path), "%s\\OOTP-KBO\\ocr\\tesseract.exe", local_app_data);
        if (GetFileAttributesA(bundled_path) != INVALID_FILE_ATTRIBUTES) {
            snprintf(out, out_size, "%s", bundled_path);
            return 1;
        }
    }

    static const char* candidates[] = {
        "C:\\Program Files\\Tesseract-OCR\\tesseract.exe",
        "C:\\Program Files (x86)\\Tesseract-OCR\\tesseract.exe"
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (GetFileAttributesA(candidates[i]) != INVALID_FILE_ATTRIBUTES) {
            snprintf(out, out_size, "%s", candidates[i]);
            return 1;
        }
    }

    DWORD found = SearchPathA(NULL, "tesseract.exe", NULL, (DWORD)out_size, out, NULL);
    return found > 0 && found < out_size;
}

static int kbo_profile_ocr_build_temp_paths(uint32_t player_id, char* bmp_path, size_t bmp_size, char* out_base, size_t base_size, char* tsv_path, size_t tsv_size)
{
    char temp_dir[MAX_PATH] = {0};
    DWORD got = GetTempPathA((DWORD)sizeof(temp_dir), temp_dir);
    if (got == 0 || got >= sizeof(temp_dir)) {
        return 0;
    }

    DWORD pid = GetCurrentProcessId();
    ULONGLONG tick = GetTickCount64();
    snprintf(out_base, base_size, "%sKBOProfileOcr-%lu-%u-%llu",
        temp_dir, (unsigned long)pid, player_id, (unsigned long long)tick);
    snprintf(bmp_path, bmp_size, "%s.bmp", out_base);
    snprintf(tsv_path, tsv_size, "%s.tsv", out_base);
    return bmp_path[0] != '\0' && out_base[0] != '\0' && tsv_path[0] != '\0';
}

static int kbo_profile_ocr_build_tessdata_dir(const char* tesseract, char* out, size_t out_size)
{
    if (tesseract == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    const char* slash = strrchr(tesseract, '\\');
    const char* fwd = strrchr(tesseract, '/');
    if (fwd != NULL && (slash == NULL || fwd > slash)) {
        slash = fwd;
    }
    if (slash == NULL || slash == tesseract) {
        return 0;
    }

    size_t dir_len = (size_t)(slash - tesseract);
    if (dir_len + strlen("\\tessdata") + 1u >= out_size) {
        return 0;
    }
    memcpy(out, tesseract, dir_len);
    out[dir_len] = '\0';
    strncat(out, "\\tessdata", out_size - strlen(out) - 1u);
    return GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES;
}

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002u
#endif

static unsigned char* kbo_profile_ocr_capture_window_pixels(HWND hwnd, int width, int height)
{
    if (hwnd == NULL || !IsWindow(hwnd) || width <= 16 || height <= 16 || width > 4096 || height > 4096) {
        append_logf("KBO player profile OCR capture skipped invalid window hwnd=%p width=%d height=%d",
            (void*)hwnd, width, height);
        return NULL;
    }

    HDC screen_dc = GetDC(NULL);
    if (screen_dc == NULL) {
        append_logf("KBO player profile OCR capture failed: GetDC error=%lu", GetLastError());
        return NULL;
    }

    HDC mem_dc = CreateCompatibleDC(screen_dc);
    if (mem_dc == NULL) {
        ReleaseDC(NULL, screen_dc);
        append_logf("KBO player profile OCR capture failed: CreateCompatibleDC error=%lu", GetLastError());
        return NULL;
    }

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = (DWORD)((SIZE_T)width * (SIZE_T)height * 4u);

    void* dib_pixels = NULL;
    HBITMAP bitmap = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS, &dib_pixels, NULL, 0);
    if (bitmap == NULL || dib_pixels == NULL) {
        DeleteDC(mem_dc);
        ReleaseDC(NULL, screen_dc);
        append_logf("KBO player profile OCR capture failed: CreateDIBSection error=%lu", GetLastError());
        return NULL;
    }

    HGDIOBJ old = SelectObject(mem_dc, bitmap);
    int copied = PrintWindow(hwnd, mem_dc, PW_RENDERFULLCONTENT);
    SelectObject(mem_dc, old);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);
    if (!copied) {
        DeleteObject(bitmap);
        append_logf("KBO player profile OCR capture failed: PrintWindow hwnd=%p error=%lu",
            (void*)hwnd, GetLastError());
        return NULL;
    }

    SIZE_T bytes = (SIZE_T)width * (SIZE_T)height * 4u;
    unsigned char* pixels = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, bytes);
    if (pixels == NULL) {
        DeleteObject(bitmap);
        append_logf("KBO player profile OCR capture failed: pixel copy allocation bytes=%llu",
            (unsigned long long)bytes);
        return NULL;
    }
    memcpy(pixels, dib_pixels, bytes);
    DeleteObject(bitmap);
    return pixels;
}

static int kbo_profile_ocr_clamp_image_rect(RECT* rect, int width, int height)
{
    if (rect == NULL || width <= 0 || height <= 0) {
        return 0;
    }
    if (rect->left < 0) { rect->left = 0; }
    if (rect->top < 0) { rect->top = 0; }
    if (rect->right > width) { rect->right = width; }
    if (rect->bottom > height) { rect->bottom = height; }
    return rect->right > rect->left && rect->bottom > rect->top;
}

static int kbo_profile_ocr_find_changed_rect(
    const unsigned char* before_pixels,
    const unsigned char* after_pixels,
    int width,
    int height,
    RECT* out_rect)
{
    if (before_pixels == NULL || after_pixels == NULL || out_rect == NULL || width <= 0 || height <= 0) {
        return 0;
    }

    int min_x = width;
    int min_y = height;
    int max_x = -1;
    int max_y = -1;
    int changed = 0;

    for (int y = 0; y < height; y += 2) {
        const unsigned char* before_row = before_pixels + (SIZE_T)y * (SIZE_T)width * 4u;
        const unsigned char* after_row = after_pixels + (SIZE_T)y * (SIZE_T)width * 4u;
        for (int x = 0; x < width; x += 2) {
            const unsigned char* b = before_row + (SIZE_T)x * 4u;
            const unsigned char* a = after_row + (SIZE_T)x * 4u;
            int db = abs((int)a[0] - (int)b[0]);
            int dg = abs((int)a[1] - (int)b[1]);
            int dr = abs((int)a[2] - (int)b[2]);
            int max_diff = db > dg ? db : dg;
            if (dr > max_diff) { max_diff = dr; }
            if (max_diff < 24 || db + dg + dr < 58) {
                continue;
            }
            if (x < min_x) { min_x = x; }
            if (y < min_y) { min_y = y; }
            if (x > max_x) { max_x = x; }
            if (y > max_y) { max_y = y; }
            changed++;
        }
    }

    if (changed < 40 || max_x < min_x || max_y < min_y) {
        return 0;
    }

    RECT rect;
    rect.left = min_x - 32;
    rect.top = min_y - 24;
    rect.right = max_x + 34;
    rect.bottom = max_y + 26;
    if (!kbo_profile_ocr_clamp_image_rect(&rect, width, height)) {
        return 0;
    }

    int rect_w = rect.right - rect.left;
    int rect_h = rect.bottom - rect.top;
    int full_pixels = width * height;
    int rect_pixels = rect_w * rect_h;
    if (rect_w < 80 || rect_h < 24 || rect_pixels > (full_pixels * 3) / 5) {
        return 0;
    }

    *out_rect = rect;
    return 1;
}

#define KBO_PROFILE_OCR_PREPROCESS_SCALE 3
#define KBO_PROFILE_OCR_TEXT_THRESHOLD 160

static int kbo_profile_ocr_save_bmp_region(
    const unsigned char* pixels,
    int width,
    int height,
    const RECT* region,
    const char* path,
    int* out_ocr_scale)
{
    if (pixels == NULL || region == NULL || path == NULL || path[0] == '\0') {
        return 0;
    }
    if (out_ocr_scale != NULL) {
        *out_ocr_scale = 1;
    }

    RECT rect = *region;
    if (!kbo_profile_ocr_clamp_image_rect(&rect, width, height)) {
        return 0;
    }

    int out_w = rect.right - rect.left;
    int out_h = rect.bottom - rect.top;
    if (out_w <= 0 || out_h <= 0 || out_w > width || out_h > height) {
        return 0;
    }

    int ocr_scale = KBO_PROFILE_OCR_PREPROCESS_SCALE;
    if (out_w > 900 || out_h > 900) {
        ocr_scale = 1;
    }
    int saved_w = out_w * ocr_scale;
    int saved_h = out_h * ocr_scale;
    DWORD image_size = (DWORD)((SIZE_T)saved_w * (SIZE_T)saved_h * 4u);
    unsigned char* row_buffer = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)saved_w * 4u);
    if (row_buffer == NULL) {
        append_logf("KBO player profile OCR crop save failed: row allocation width=%d", saved_w);
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO player profile OCR crop save failed: CreateFile path=%s error=%lu", path, GetLastError());
        HeapFree(GetProcessHeap(), 0, row_buffer);
        return 0;
    }

    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;
    memset(&bfh, 0, sizeof(bfh));
    memset(&bih, 0, sizeof(bih));
    bfh.bfType = 0x4d42u;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + image_size;
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = saved_w;
    bih.biHeight = saved_h;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = image_size;

    DWORD written = 0;
    int ok = WriteFile(file, &bfh, sizeof(bfh), &written, NULL) && written == sizeof(bfh);
    ok = ok && WriteFile(file, &bih, sizeof(bih), &written, NULL) && written == sizeof(bih);
    for (int y = saved_h - 1; ok && y >= 0; y--) {
        int src_y = rect.top + (y / ocr_scale);
        const unsigned char* src_row = pixels + (SIZE_T)src_y * (SIZE_T)width * 4u;
        for (int x = 0; x < saved_w; x++) {
            int src_x = rect.left + (x / ocr_scale);
            const unsigned char* px = src_row + (SIZE_T)src_x * 4u;
            int gray = ((int)px[2] * 30 + (int)px[1] * 59 + (int)px[0] * 11) / 100;
            unsigned char value = gray >= KBO_PROFILE_OCR_TEXT_THRESHOLD ? 0u : 255u;
            row_buffer[(SIZE_T)x * 4u + 0u] = value;
            row_buffer[(SIZE_T)x * 4u + 1u] = value;
            row_buffer[(SIZE_T)x * 4u + 2u] = value;
            row_buffer[(SIZE_T)x * 4u + 3u] = 0u;
        }
        DWORD row_bytes = (DWORD)((SIZE_T)saved_w * 4u);
        ok = WriteFile(file, row_buffer, row_bytes, &written, NULL) && written == row_bytes;
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, row_buffer);

    if (!ok) {
        DeleteFileA(path);
        append_logf("KBO player profile OCR crop save failed: WriteFile path=%s error=%lu", path, GetLastError());
        return 0;
    }
    if (out_ocr_scale != NULL) {
        *out_ocr_scale = ocr_scale;
    }

    append_logf("KBO player profile OCR crop saved path=%s crop=%ld,%ld,%ld,%ld size=%dx%d ocr_size=%dx%d scale=%d threshold=%d full=%dx%d method=PrintWindowDiff",
        path, rect.left, rect.top, rect.right, rect.bottom, out_w, out_h,
        saved_w, saved_h, ocr_scale, KBO_PROFILE_OCR_TEXT_THRESHOLD, width, height);
    return 1;
}

static int kbo_profile_ocr_run_tesseract(const char* tesseract, const char* bmp_path, const char* out_base, DWORD timeout_ms)
{
    char q_tesseract[MAX_PATH + 8] = {0};
    char q_bmp[MAX_PATH + 8] = {0};
    char q_base[MAX_PATH + 8] = {0};
    char tessdata_dir[MAX_PATH] = {0};
    char q_tessdata_dir[MAX_PATH + 8] = {0};
    char command[2048] = {0};

    kbo_profile_ocr_quote_arg(tesseract, q_tesseract, sizeof(q_tesseract));
    kbo_profile_ocr_quote_arg(bmp_path, q_bmp, sizeof(q_bmp));
    kbo_profile_ocr_quote_arg(out_base, q_base, sizeof(q_base));
    if (kbo_profile_ocr_build_tessdata_dir(tesseract, tessdata_dir, sizeof(tessdata_dir))) {
        kbo_profile_ocr_quote_arg(tessdata_dir, q_tessdata_dir, sizeof(q_tessdata_dir));
        snprintf(command, sizeof(command), "%s %s %s --tessdata-dir %s -l eng --psm 6 --dpi 96 tsv",
            q_tesseract, q_bmp, q_base, q_tessdata_dir);
    } else {
        snprintf(command, sizeof(command), "%s %s %s -l eng --psm 6 --dpi 96 tsv", q_tesseract, q_bmp, q_base);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        append_logf("KBO player profile OCR tesseract start failed error=%lu cmd=%s", GetLastError(), command);
        return 0;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, timeout_ms);
    DWORD exit_code = 0xffffffffu;
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
        append_logf("KBO player profile OCR tesseract timeout timeout_ms=%lu", (unsigned long)timeout_ms);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (wait != WAIT_OBJECT_0 || exit_code != 0) {
        append_logf("KBO player profile OCR tesseract failed wait=%lu exit=%lu", (unsigned long)wait, (unsigned long)exit_code);
        return 0;
    }

    append_logf("KBO player profile OCR tesseract done path=%s", bmp_path);
    return 1;
}

static int kbo_profile_ocr_load_file(const char* path, char** out_data, DWORD* out_size)
{
    if (out_data == NULL || out_size == NULL) {
        return 0;
    }
    *out_data = NULL;
    *out_size = 0;

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0 || size > (4u * 1024u * 1024u)) {
        CloseHandle(file);
        return 0;
    }
    char* data = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (data == NULL) {
        CloseHandle(file);
        return 0;
    }
    DWORD read = 0;
    int ok = ReadFile(file, data, size, &read, NULL) && read == size;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, data);
        return 0;
    }
    data[size] = '\0';
    *out_data = data;
    *out_size = size;
    return 1;
}

static int kbo_profile_ocr_parse_tsv_word_line(
    char* line,
    KboPlayerProfileOcrWord* words,
    int* word_count,
    int max_words)
{
    char* fields[12] = {0};
    int field_count = 0;
    char* cursor = line;
    fields[field_count++] = cursor;
    while (*cursor != '\0' && field_count < 12) {
        if (*cursor == '\t') {
            *cursor = '\0';
            fields[field_count++] = cursor + 1;
        }
        cursor++;
    }

    if (field_count < 12 || word_count == NULL || *word_count >= max_words) {
        return 0;
    }

    int level = atoi(fields[0]);
    if (level != 5) {
        return 0;
    }

    char norm[48] = {0};
    kbo_profile_ocr_normalize_word(fields[11], norm, sizeof(norm));
    if (norm[0] == '\0') {
        return 0;
    }

    double conf = strtod(fields[10], NULL);
    if (conf < 0.0) {
        return 0;
    }

    KboPlayerProfileOcrWord* word = &words[*word_count];
    memset(word, 0, sizeof(*word));
    word->left = atoi(fields[6]);
    word->top = atoi(fields[7]);
    int width = atoi(fields[8]);
    int height = atoi(fields[9]);
    word->right = word->left + width;
    word->bottom = word->top + height;
    word->line_key = atoi(fields[2]) * 1000000 + atoi(fields[3]) * 1000 + atoi(fields[4]);
    word->conf = conf;
    snprintf(word->raw, sizeof(word->raw), "%s", fields[11]);
    snprintf(word->norm, sizeof(word->norm), "%s", norm);
    (*word_count)++;
    return 1;
}

static void kbo_profile_ocr_union_words(
    const KboPlayerProfileOcrWord* a,
    const KboPlayerProfileOcrWord* b,
    const KboPlayerProfileOcrWord* c,
    RECT* out)
{
    out->left = a->left;
    out->top = a->top;
    out->right = a->right;
    out->bottom = a->bottom;
    const KboPlayerProfileOcrWord* items[2] = { b, c };
    for (int i = 0; i < 2; i++) {
        const KboPlayerProfileOcrWord* word = items[i];
        if (word == NULL) {
            continue;
        }
        if (word->left < out->left) { out->left = word->left; }
        if (word->top < out->top) { out->top = word->top; }
        if (word->right > out->right) { out->right = word->right; }
        if (word->bottom > out->bottom) { out->bottom = word->bottom; }
    }
}

static void kbo_profile_ocr_build_sample(
    const KboPlayerProfileOcrWord* words,
    int word_count,
    char* sample,
    size_t sample_size)
{
    if (sample == NULL || sample_size == 0) {
        return;
    }
    sample[0] = '\0';
    if (words == NULL || word_count <= 0) {
        return;
    }

    size_t used = 0;
    int limit = word_count < 18 ? word_count : 18;
    for (int i = 0; i < limit && used + 8 < sample_size; i++) {
        int wrote = snprintf(sample + used, sample_size - used,
            "%s%s/%s@%.0f[%d,%d,%d,%d]",
            used > 0 ? " " : "",
            words[i].raw,
            words[i].norm,
            words[i].conf,
            words[i].left,
            words[i].top,
            words[i].right,
            words[i].bottom);
        if (wrote <= 0) {
            break;
        }
        if ((size_t)wrote >= sample_size - used) {
            used = sample_size - 1u;
            break;
        }
        used += (size_t)wrote;
    }
}

static int kbo_profile_ocr_contains_quit_confirm_modal(
    const KboPlayerProfileOcrWord* words,
    int word_count)
{
    if (words == NULL || word_count <= 0) {
        return 0;
    }

    int saw_are = 0;
    int saw_sure = 0;
    int saw_want = 0;
    int saw_quit = 0;
    int saw_program = 0;
    int saw_yes = 0;
    int saw_no = 0;
    for (int i = 0; i < word_count; i++) {
        const KboPlayerProfileOcrWord* word = &words[i];
        if (word->conf < 10.0 || word->norm[0] == '\0') {
            continue;
        }
        if (kbo_profile_ocr_word_matches(word->norm, "are")) {
            saw_are = 1;
        } else if (kbo_profile_ocr_word_matches(word->norm, "sure")) {
            saw_sure = 1;
        } else if (kbo_profile_ocr_word_matches(word->norm, "want")) {
            saw_want = 1;
        } else if (kbo_profile_ocr_word_matches(word->norm, "quit")) {
            saw_quit = 1;
        } else if (kbo_profile_ocr_word_matches(word->norm, "program")) {
            saw_program = 1;
        } else if (strcmp(word->norm, "yes") == 0) {
            saw_yes = 1;
        } else if (strcmp(word->norm, "no") == 0) {
            saw_no = 1;
        }
    }

    return (saw_quit && saw_program && saw_yes && saw_no)
        || (saw_are && saw_sure && saw_want && saw_quit);
}

static int kbo_profile_ocr_find_target_in_tsv(
    const char* tsv_path,
    uint32_t player_id,
    POINT* image_point,
    char* phrase_out,
    size_t phrase_size)
{
    if (image_point == NULL) {
        return 0;
    }
    image_point->x = 0;
    image_point->y = 0;
    if (phrase_out != NULL && phrase_size > 0) {
        phrase_out[0] = '\0';
    }

    char* data = NULL;
    DWORD data_size = 0;
    if (!kbo_profile_ocr_load_file(tsv_path, &data, &data_size)) {
        append_logf("KBO player profile OCR TSV load failed player=%u path=%s", player_id, tsv_path);
        return 0;
    }

    KboPlayerProfileOcrWord words[512];
    int word_count = 0;
    char* line = data;
    int line_index = 0;
    for (char* p = data; ; p++) {
        if (*p == '\r' || *p == '\n' || *p == '\0') {
            char saved = *p;
            *p = '\0';
            if (line[0] != '\0' && line_index > 0) {
                kbo_profile_ocr_parse_tsv_word_line(line, words, &word_count, (int)(sizeof(words) / sizeof(words[0])));
            }
            line_index++;
            if (saved == '\0') {
                break;
            }
            if (saved == '\r' && p[1] == '\n') {
                p++;
            }
            line = p + 1;
        }
    }

    if (kbo_profile_ocr_contains_quit_confirm_modal(words, word_count)) {
        char sample[512] = {0};
        kbo_profile_ocr_build_sample(words, word_count, sample, sizeof(sample));
        append_logf("KBO player profile OCR blocking modal detected player=%u kind=quit_confirm words=%d sample=\"%s\" path=%s",
            player_id, word_count, sample, tsv_path);
        HeapFree(GetProcessHeap(), 0, data);
        return -1;
    }

    double best_score = 0.0;
    RECT best_rect = {0};
    char best_phrase[64] = {0};
    for (int i = 0; i < word_count; i++) {
        KboPlayerProfileOcrWord* w0 = &words[i];
        if (w0->conf < 15.0) {
            continue;
        }

        if (kbo_profile_ocr_word_matches(w0->norm, "open")) {
            for (int j = i + 1; j < word_count && j <= i + 4; j++) {
                KboPlayerProfileOcrWord* w1 = &words[j];
                if (w1->line_key != w0->line_key || w1->conf < 15.0) {
                    continue;
                }
                if (!kbo_profile_ocr_word_matches(w1->norm, "player")) {
                    continue;
                }
                for (int k = j + 1; k < word_count && k <= j + 4; k++) {
                    KboPlayerProfileOcrWord* w2 = &words[k];
                    if (w2->line_key != w0->line_key || w2->conf < 15.0) {
                        continue;
                    }
                    if (!kbo_profile_ocr_word_matches(w2->norm, "profile")) {
                        continue;
                    }
                    double score = 300.0 + (w0->conf + w1->conf + w2->conf) / 3.0;
                    if (score > best_score) {
                        best_score = score;
                        kbo_profile_ocr_union_words(w0, w1, w2, &best_rect);
                        snprintf(best_phrase, sizeof(best_phrase), "%s %s %s", w0->raw, w1->raw, w2->raw);
                    }
                }
            }
        }

        if (kbo_profile_ocr_word_matches(w0->norm, "player")) {
            for (int j = i + 1; j < word_count && j <= i + 3; j++) {
                KboPlayerProfileOcrWord* w1 = &words[j];
                if (w1->line_key != w0->line_key || w1->conf < 20.0) {
                    continue;
                }
                if (!kbo_profile_ocr_word_matches(w1->norm, "profile")) {
                    continue;
                }
                double score = 200.0 + (w0->conf + w1->conf) / 2.0;
                if (score > best_score) {
                    best_score = score;
                    kbo_profile_ocr_union_words(w0, w1, NULL, &best_rect);
                    snprintf(best_phrase, sizeof(best_phrase), "%s %s", w0->raw, w1->raw);
                }
            }
        }
    }

    if (best_score <= 0.0) {
        char sample[512] = {0};
        kbo_profile_ocr_build_sample(words, word_count, sample, sizeof(sample));
        append_logf("KBO player profile OCR target missed player=%u words=%d sample=\"%s\" path=%s",
            player_id, word_count, sample, tsv_path);
        HeapFree(GetProcessHeap(), 0, data);
        return 0;
    }

    HeapFree(GetProcessHeap(), 0, data);

    image_point->x = (best_rect.left + best_rect.right) / 2;
    image_point->y = (best_rect.top + best_rect.bottom) / 2;
    if (phrase_out != NULL && phrase_size > 0) {
        snprintf(phrase_out, phrase_size, "%s", best_phrase);
    }
    append_logf("KBO player profile OCR target hit player=%u phrase=\"%s\" image=%ld,%ld rect=%ld,%ld,%ld,%ld score=%.1f",
        player_id, best_phrase, image_point->x, image_point->y,
        best_rect.left, best_rect.top, best_rect.right, best_rect.bottom, best_score);
    return 1;
}

static LPARAM kbo_profile_ocr_mouse_lparam(int x, int y)
{
    return (LPARAM)((((DWORD)y) & 0xffffu) << 16 | (((DWORD)x) & 0xffffu));
}

typedef BOOL (WINAPI *KboGetCursorPosFn)(LPPOINT);
typedef BOOL (WINAPI *KboGetCursorInfoFn)(PCURSORINFO);
typedef DWORD (WINAPI *KboGetMessagePosFn)(void);
typedef SHORT (WINAPI *KboGetKeyStateFn)(int);
typedef SHORT (WINAPI *KboGetAsyncKeyStateFn)(int);
typedef HWND (WINAPI *KboGetWindowNoArgFn)(void);
typedef HWND (WINAPI *KboWindowFromPointFn)(POINT);
typedef BOOL (WINAPI *KboPeekMessageWFn)(LPMSG, HWND, UINT, UINT, UINT);
typedef LRESULT (WINAPI *KboDispatchMessageWFn)(const MSG*);
typedef BOOL (WINAPI *KboTranslateMessageFn)(const MSG*);

static KboGetCursorPosFn g_kbo_player_profile_ocr_real_get_cursor_pos = NULL;
static KboGetCursorInfoFn g_kbo_player_profile_ocr_real_get_cursor_info = NULL;
static KboGetMessagePosFn g_kbo_player_profile_ocr_real_get_message_pos = NULL;
static KboGetKeyStateFn g_kbo_player_profile_ocr_real_get_key_state = NULL;
static KboGetAsyncKeyStateFn g_kbo_player_profile_ocr_real_get_async_key_state = NULL;
static KboGetCursorPosFn g_kbo_player_profile_ocr_real_get_physical_cursor_pos = NULL;
static KboGetWindowNoArgFn g_kbo_player_profile_ocr_real_get_foreground_window = NULL;
static KboGetWindowNoArgFn g_kbo_player_profile_ocr_real_get_active_window = NULL;
static KboGetWindowNoArgFn g_kbo_player_profile_ocr_real_get_focus = NULL;
static KboGetWindowNoArgFn g_kbo_player_profile_ocr_real_get_capture = NULL;
static KboWindowFromPointFn g_kbo_player_profile_ocr_real_window_from_point = NULL;
static KboPeekMessageWFn g_kbo_player_profile_ocr_real_peek_message_w = NULL;
static KboDispatchMessageWFn g_kbo_player_profile_ocr_real_dispatch_message_w = NULL;
static KboTranslateMessageFn g_kbo_player_profile_ocr_real_translate_message = NULL;

static HWND kbo_profile_ocr_virtual_mouse_hwnd(void)
{
    if (InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_active, 0, 0) == 0) {
        return NULL;
    }
    HWND hwnd = (HWND)InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_kbo_player_profile_ocr_virtual_mouse_hwnd,
        NULL,
        NULL);
    return hwnd != NULL && IsWindow(hwnd) ? hwnd : NULL;
}

static int kbo_profile_ocr_virtual_mouse_point(POINT* point)
{
    if (point == NULL || InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_active, 0, 0) == 0) {
        return 0;
    }
    point->x = InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_x, 0, 0);
    point->y = InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_y, 0, 0);
    return 1;
}

static BOOL WINAPI kbo_profile_ocr_hook_get_cursor_pos(LPPOINT point)
{
    if (kbo_profile_ocr_virtual_mouse_point(point)) {
        return TRUE;
    }
    return g_kbo_player_profile_ocr_real_get_cursor_pos != NULL
        ? g_kbo_player_profile_ocr_real_get_cursor_pos(point)
        : FALSE;
}

static BOOL WINAPI kbo_profile_ocr_hook_get_physical_cursor_pos(LPPOINT point)
{
    if (kbo_profile_ocr_virtual_mouse_point(point)) {
        return TRUE;
    }
    return g_kbo_player_profile_ocr_real_get_physical_cursor_pos != NULL
        ? g_kbo_player_profile_ocr_real_get_physical_cursor_pos(point)
        : FALSE;
}

static BOOL WINAPI kbo_profile_ocr_hook_get_cursor_info(PCURSORINFO info)
{
    BOOL ok = g_kbo_player_profile_ocr_real_get_cursor_info != NULL
        ? g_kbo_player_profile_ocr_real_get_cursor_info(info)
        : FALSE;
    POINT point;
    if (ok && info != NULL && kbo_profile_ocr_virtual_mouse_point(&point)) {
        info->ptScreenPos = point;
    }
    return ok;
}

static DWORD WINAPI kbo_profile_ocr_hook_get_message_pos(void)
{
    POINT point;
    if (kbo_profile_ocr_virtual_mouse_point(&point)) {
        return (((DWORD)point.y & 0xffffu) << 16) | ((DWORD)point.x & 0xffffu);
    }
    return g_kbo_player_profile_ocr_real_get_message_pos != NULL
        ? g_kbo_player_profile_ocr_real_get_message_pos()
        : 0u;
}

static SHORT WINAPI kbo_profile_ocr_hook_get_key_state(int key)
{
    if (key == VK_LBUTTON
            && InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_active, 0, 0) != 0) {
        return InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_left_down, 0, 0) != 0
            ? (SHORT)0x8000
            : (SHORT)0;
    }
    return g_kbo_player_profile_ocr_real_get_key_state != NULL
        ? g_kbo_player_profile_ocr_real_get_key_state(key)
        : (SHORT)0;
}

static SHORT WINAPI kbo_profile_ocr_hook_get_async_key_state(int key)
{
    if (key == VK_LBUTTON
            && InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_active, 0, 0) != 0) {
        return InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_left_down, 0, 0) != 0
            ? (SHORT)0x8001
            : (SHORT)0;
    }
    return g_kbo_player_profile_ocr_real_get_async_key_state != NULL
        ? g_kbo_player_profile_ocr_real_get_async_key_state(key)
        : (SHORT)0;
}

static HWND WINAPI kbo_profile_ocr_hook_get_foreground_window(void)
{
    HWND hwnd = kbo_profile_ocr_virtual_mouse_hwnd();
    if (hwnd != NULL) {
        return hwnd;
    }
    return g_kbo_player_profile_ocr_real_get_foreground_window != NULL
        ? g_kbo_player_profile_ocr_real_get_foreground_window()
        : NULL;
}

static HWND WINAPI kbo_profile_ocr_hook_get_active_window(void)
{
    HWND hwnd = kbo_profile_ocr_virtual_mouse_hwnd();
    if (hwnd != NULL) {
        return hwnd;
    }
    return g_kbo_player_profile_ocr_real_get_active_window != NULL
        ? g_kbo_player_profile_ocr_real_get_active_window()
        : NULL;
}

static HWND WINAPI kbo_profile_ocr_hook_get_focus(void)
{
    HWND hwnd = kbo_profile_ocr_virtual_mouse_hwnd();
    if (hwnd != NULL) {
        return hwnd;
    }
    return g_kbo_player_profile_ocr_real_get_focus != NULL
        ? g_kbo_player_profile_ocr_real_get_focus()
        : NULL;
}

static HWND WINAPI kbo_profile_ocr_hook_get_capture(void)
{
    HWND hwnd = kbo_profile_ocr_virtual_mouse_hwnd();
    if (hwnd != NULL
            && InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_left_down, 0, 0) != 0) {
        return hwnd;
    }
    return g_kbo_player_profile_ocr_real_get_capture != NULL
        ? g_kbo_player_profile_ocr_real_get_capture()
        : NULL;
}

static HWND WINAPI kbo_profile_ocr_hook_window_from_point(POINT point)
{
    HWND hwnd = kbo_profile_ocr_virtual_mouse_hwnd();
    POINT virtual_point;
    if (hwnd != NULL && kbo_profile_ocr_virtual_mouse_point(&virtual_point)) {
        RECT rect;
        if (GetWindowRect(hwnd, &rect)
                && virtual_point.x >= rect.left && virtual_point.x < rect.right
                && virtual_point.y >= rect.top && virtual_point.y < rect.bottom
                && abs(point.x - virtual_point.x) <= 3
                && abs(point.y - virtual_point.y) <= 3) {
            return hwnd;
        }
    }
    return g_kbo_player_profile_ocr_real_window_from_point != NULL
        ? g_kbo_player_profile_ocr_real_window_from_point(point)
        : NULL;
}

static int kbo_profile_ocr_rewrite_mouse_msg(MSG* msg, const char* source)
{
    if (msg == NULL) {
        return 0;
    }

    HWND virtual_hwnd = kbo_profile_ocr_virtual_mouse_hwnd();
    if (virtual_hwnd == NULL) {
        return 0;
    }

    UINT message = msg->message;
    if (message != WM_MOUSEMOVE
            && message != WM_LBUTTONDOWN
            && message != WM_LBUTTONUP
            && message != WM_LBUTTONDBLCLK) {
        return 0;
    }

    POINT screen_point;
    if (!kbo_profile_ocr_virtual_mouse_point(&screen_point)) {
        return 0;
    }
    if (msg->hwnd != NULL && msg->hwnd != virtual_hwnd && !IsChild(virtual_hwnd, msg->hwnd)) {
        return 0;
    }

    HWND target_hwnd = msg->hwnd != NULL ? msg->hwnd : virtual_hwnd;
    POINT client_point = screen_point;
    if (!ScreenToClient(target_hwnd, &client_point)) {
        return 0;
    }

    msg->hwnd = target_hwnd;
    msg->pt = screen_point;
    msg->time = GetTickCount();
    msg->lParam = kbo_profile_ocr_mouse_lparam((int)client_point.x, (int)client_point.y);
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK
            || InterlockedCompareExchange(&g_kbo_player_profile_ocr_virtual_mouse_left_down, 0, 0) != 0) {
        msg->wParam |= MK_LBUTTON;
    } else if (message == WM_LBUTTONUP) {
        msg->wParam &= ~((WPARAM)MK_LBUTTON);
    }

    LONG log_count = InterlockedIncrement(&g_kbo_player_profile_ocr_virtual_mouse_rewrite_log_count);
    if (log_count <= 12) {
        append_logf("KBO player profile OCR mouse msg rewrite source=%s msg=0x%x hwnd=%p screen=%ld,%ld client=%ld,%ld wparam=0x%Ix",
            source != NULL ? source : "",
            message,
            (void*)target_hwnd,
            screen_point.x,
            screen_point.y,
            client_point.x,
            client_point.y,
            (size_t)msg->wParam);
    }
    return 1;
}

static BOOL WINAPI kbo_profile_ocr_hook_peek_message_w(LPMSG msg, HWND hwnd, UINT filter_min, UINT filter_max, UINT remove_msg)
{
    BOOL ok = g_kbo_player_profile_ocr_real_peek_message_w != NULL
        ? g_kbo_player_profile_ocr_real_peek_message_w(msg, hwnd, filter_min, filter_max, remove_msg)
        : FALSE;
    if (ok && msg != NULL) {
        kbo_profile_ocr_rewrite_mouse_msg(msg, "peek");
    }
    return ok;
}

static LRESULT WINAPI kbo_profile_ocr_hook_dispatch_message_w(const MSG* msg)
{
    if (msg != NULL) {
        MSG rewritten = *msg;
        kbo_profile_ocr_rewrite_mouse_msg(&rewritten, "dispatch");
        return g_kbo_player_profile_ocr_real_dispatch_message_w != NULL
            ? g_kbo_player_profile_ocr_real_dispatch_message_w(&rewritten)
            : 0;
    }
    return g_kbo_player_profile_ocr_real_dispatch_message_w != NULL
        ? g_kbo_player_profile_ocr_real_dispatch_message_w(msg)
        : 0;
}

static BOOL WINAPI kbo_profile_ocr_hook_translate_message(const MSG* msg)
{
    if (msg != NULL) {
        MSG rewritten = *msg;
        kbo_profile_ocr_rewrite_mouse_msg(&rewritten, "translate");
        return g_kbo_player_profile_ocr_real_translate_message != NULL
            ? g_kbo_player_profile_ocr_real_translate_message(&rewritten)
            : FALSE;
    }
    return g_kbo_player_profile_ocr_real_translate_message != NULL
        ? g_kbo_player_profile_ocr_real_translate_message(msg)
        : FALSE;
}

static int kbo_profile_ocr_hook_iat_func(HMODULE module, const char* dll_name, const char* func_name, void* replacement, void** original_out)
{
    if (module == NULL || dll_name == NULL || func_name == NULL || replacement == NULL) {
        return 0;
    }

    unsigned char* base = (unsigned char*)module;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (dir.VirtualAddress == 0 || dir.Size == 0) {
        return 0;
    }

    int hooked = 0;
    IMAGE_IMPORT_DESCRIPTOR* imports = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress);
    for (; imports->Name != 0; imports++) {
        const char* imported_dll = (const char*)(base + imports->Name);
        if (lstrcmpiA(imported_dll, dll_name) != 0) {
            continue;
        }

        IMAGE_THUNK_DATA* names = imports->OriginalFirstThunk != 0
            ? (IMAGE_THUNK_DATA*)(base + imports->OriginalFirstThunk)
            : NULL;
        IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)(base + imports->FirstThunk);
        if (names == NULL || thunk == NULL) {
            continue;
        }

        for (; names->u1.AddressOfData != 0; names++, thunk++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) {
                continue;
            }
            IMAGE_IMPORT_BY_NAME* import_name = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (strcmp((const char*)import_name->Name, func_name) != 0) {
                continue;
            }

            void** slot = (void**)&thunk->u1.Function;
            if (*slot == replacement) {
                hooked++;
                continue;
            }
            if (original_out != NULL && *original_out == NULL) {
                *original_out = *slot;
            }

            DWORD old_protect = 0;
            if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protect)) {
                append_logf("KBO player profile OCR virtual mouse hook failed: VirtualProtect func=%s error=%lu",
                    func_name, GetLastError());
                continue;
            }
            *slot = replacement;
            FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
            DWORD ignored = 0;
            VirtualProtect(slot, sizeof(*slot), old_protect, &ignored);
            hooked++;
        }
    }
    return hooked;
}

static int kbo_profile_ocr_path_starts_with_i(const char* path, const char* prefix)
{
    if (path == NULL || prefix == NULL || prefix[0] == '\0') {
        return 0;
    }
    size_t prefix_len = strlen(prefix);
    return _strnicmp(path, prefix, prefix_len) == 0;
}

static const char* kbo_profile_ocr_basename(const char* path)
{
    if (path == NULL) {
        return "";
    }
    const char* slash = strrchr(path, '\\');
    const char* fslash = strrchr(path, '/');
    const char* base = slash > fslash ? slash : fslash;
    return base != NULL ? base + 1 : path;
}

static int kbo_profile_ocr_is_windows_module_path(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    char windows_dir[MAX_PATH] = {0};
    DWORD got = GetWindowsDirectoryA(windows_dir, (UINT)sizeof(windows_dir));
    if (got == 0 || got >= sizeof(windows_dir)) {
        return 0;
    }
    size_t len = strlen(windows_dir);
    while (len > 0 && (windows_dir[len - 1] == '\\' || windows_dir[len - 1] == '/')) {
        windows_dir[--len] = '\0';
    }

    char windows_prefix[MAX_PATH] = {0};
    snprintf(windows_prefix, sizeof(windows_prefix), "%s\\", windows_dir);
    return kbo_profile_ocr_path_starts_with_i(path, windows_prefix);
}

static int kbo_profile_ocr_should_skip_hook_module(HMODULE module, HMODULE self_module, const char* path)
{
    if (module == NULL) {
        return 1;
    }
    if (self_module != NULL && module == self_module) {
        return 1;
    }
    if (kbo_profile_ocr_is_windows_module_path(path)) {
        return 1;
    }

    const char* base = kbo_profile_ocr_basename(path);
    if (base[0] == '\0') {
        return 0;
    }
    if (lstrcmpiA(base, "WebView2Loader.dll") == 0) {
        return 1;
    }
    return 0;
}

static int kbo_profile_ocr_hook_virtual_mouse_module(HMODULE module)
{
    int hooks = 0;
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetCursorPos",
        (void*)kbo_profile_ocr_hook_get_cursor_pos,
        (void**)&g_kbo_player_profile_ocr_real_get_cursor_pos);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetPhysicalCursorPos",
        (void*)kbo_profile_ocr_hook_get_physical_cursor_pos,
        (void**)&g_kbo_player_profile_ocr_real_get_physical_cursor_pos);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetCursorInfo",
        (void*)kbo_profile_ocr_hook_get_cursor_info,
        (void**)&g_kbo_player_profile_ocr_real_get_cursor_info);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetMessagePos",
        (void*)kbo_profile_ocr_hook_get_message_pos,
        (void**)&g_kbo_player_profile_ocr_real_get_message_pos);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetKeyState",
        (void*)kbo_profile_ocr_hook_get_key_state,
        (void**)&g_kbo_player_profile_ocr_real_get_key_state);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetAsyncKeyState",
        (void*)kbo_profile_ocr_hook_get_async_key_state,
        (void**)&g_kbo_player_profile_ocr_real_get_async_key_state);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetForegroundWindow",
        (void*)kbo_profile_ocr_hook_get_foreground_window,
        (void**)&g_kbo_player_profile_ocr_real_get_foreground_window);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetActiveWindow",
        (void*)kbo_profile_ocr_hook_get_active_window,
        (void**)&g_kbo_player_profile_ocr_real_get_active_window);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetFocus",
        (void*)kbo_profile_ocr_hook_get_focus,
        (void**)&g_kbo_player_profile_ocr_real_get_focus);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "GetCapture",
        (void*)kbo_profile_ocr_hook_get_capture,
        (void**)&g_kbo_player_profile_ocr_real_get_capture);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "WindowFromPoint",
        (void*)kbo_profile_ocr_hook_window_from_point,
        (void**)&g_kbo_player_profile_ocr_real_window_from_point);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "PeekMessageW",
        (void*)kbo_profile_ocr_hook_peek_message_w,
        (void**)&g_kbo_player_profile_ocr_real_peek_message_w);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "DispatchMessageW",
        (void*)kbo_profile_ocr_hook_dispatch_message_w,
        (void**)&g_kbo_player_profile_ocr_real_dispatch_message_w);
    hooks += kbo_profile_ocr_hook_iat_func(module, "USER32.dll", "TranslateMessage",
        (void*)kbo_profile_ocr_hook_translate_message,
        (void**)&g_kbo_player_profile_ocr_real_translate_message);
    return hooks;
}

static int kbo_profile_ocr_install_virtual_mouse_hooks(void)
{
    LONG previous = InterlockedCompareExchange(&g_kbo_player_profile_ocr_mouse_hook_installed, 1, 0);
    if (previous != 0) {
        return 1;
    }

    int hooks = 0;
    int modules_scanned = 0;
    int modules_skipped = 0;
    int modules_hooked = 0;
    char hooked_modules[512] = {0};
    size_t hooked_len = 0;

    HMODULE self_module = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&kbo_profile_ocr_install_virtual_mouse_hooks,
        &self_module);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 entry;
        ZeroMemory(&entry, sizeof(entry));
        entry.dwSize = sizeof(entry);
        if (Module32First(snapshot, &entry)) {
            do {
                HMODULE module = (HMODULE)entry.modBaseAddr;
                const char* path = entry.szExePath[0] != '\0' ? entry.szExePath : entry.szModule;
                modules_scanned++;
                if (kbo_profile_ocr_should_skip_hook_module(module, self_module, path)) {
                    modules_skipped++;
                    continue;
                }

                int module_hooks = kbo_profile_ocr_hook_virtual_mouse_module(module);
                if (module_hooks > 0) {
                    modules_hooked++;
                    if (hooked_len + 32 < sizeof(hooked_modules)) {
                        int wrote = snprintf(
                            hooked_modules + hooked_len,
                            sizeof(hooked_modules) - hooked_len,
                            "%s:%d ",
                            kbo_profile_ocr_basename(path),
                            module_hooks);
                        if (wrote > 0) {
                            hooked_len += (size_t)wrote;
                            if (hooked_len >= sizeof(hooked_modules)) {
                                hooked_len = sizeof(hooked_modules) - 1;
                                hooked_modules[hooked_len] = '\0';
                            }
                        }
                    }
                    hooks += module_hooks;
                }
            } while (Module32Next(snapshot, &entry));
        } else {
            append_logf("KBO player profile OCR virtual mouse module scan failed: Module32First error=%lu",
                GetLastError());
        }
        CloseHandle(snapshot);
    } else {
        append_logf("KBO player profile OCR virtual mouse module scan failed: snapshot error=%lu",
            GetLastError());
    }

    if (hooks <= 0) {
        HMODULE exe = GetModuleHandleA(NULL);
        hooks += kbo_profile_ocr_hook_virtual_mouse_module(exe);
        if (hooks <= 0) {
            InterlockedExchange(&g_kbo_player_profile_ocr_mouse_hook_installed, 0);
        }
    }

    append_logf("KBO player profile OCR virtual mouse hooks installed hooks=%d modules=%d hooked_modules=%d skipped=%d list=\"%s\" cursor=%p physical=%p cursor_info=%p msgpos=%p keystate=%p async=%p foreground=%p active=%p focus=%p capture=%p window_from_point=%p peek=%p dispatch=%p translate=%p",
        hooks,
        modules_scanned,
        modules_hooked,
        modules_skipped,
        hooked_modules,
        (void*)g_kbo_player_profile_ocr_real_get_cursor_pos,
        (void*)g_kbo_player_profile_ocr_real_get_physical_cursor_pos,
        (void*)g_kbo_player_profile_ocr_real_get_cursor_info,
        (void*)g_kbo_player_profile_ocr_real_get_message_pos,
        (void*)g_kbo_player_profile_ocr_real_get_key_state,
        (void*)g_kbo_player_profile_ocr_real_get_async_key_state,
        (void*)g_kbo_player_profile_ocr_real_get_foreground_window,
        (void*)g_kbo_player_profile_ocr_real_get_active_window,
        (void*)g_kbo_player_profile_ocr_real_get_focus,
        (void*)g_kbo_player_profile_ocr_real_get_capture,
        (void*)g_kbo_player_profile_ocr_real_window_from_point,
        (void*)g_kbo_player_profile_ocr_real_peek_message_w,
        (void*)g_kbo_player_profile_ocr_real_dispatch_message_w,
        (void*)g_kbo_player_profile_ocr_real_translate_message);
    return hooks > 0;
}

static void kbo_profile_ocr_virtual_mouse_set(HWND hwnd, int screen_x, int screen_y, int left_down)
{
    InterlockedExchangePointer(
        (PVOID volatile*)&g_kbo_player_profile_ocr_virtual_mouse_hwnd,
        (PVOID)hwnd);
    InterlockedExchange(&g_kbo_player_profile_ocr_virtual_mouse_x, screen_x);
    InterlockedExchange(&g_kbo_player_profile_ocr_virtual_mouse_y, screen_y);
    InterlockedExchange(&g_kbo_player_profile_ocr_virtual_mouse_left_down, left_down ? 1 : 0);
    if (InterlockedExchange(&g_kbo_player_profile_ocr_virtual_mouse_active, 1) == 0) {
        InterlockedExchange(&g_kbo_player_profile_ocr_virtual_mouse_rewrite_log_count, 0);
    }
}

static void kbo_profile_ocr_virtual_mouse_clear(void)
{
    InterlockedExchange(&g_kbo_player_profile_ocr_virtual_mouse_left_down, 0);
    InterlockedExchange(&g_kbo_player_profile_ocr_virtual_mouse_active, 0);
    InterlockedExchangePointer(
        (PVOID volatile*)&g_kbo_player_profile_ocr_virtual_mouse_hwnd,
        NULL);
}

static int kbo_profile_ocr_send_click(HWND hwnd, int screen_x, int screen_y, uint32_t player_id, const char* phrase);

static int kbo_profile_ocr_crop_looks_like_menu(const RECT* crop_rect, int full_width, int full_height)
{
    if (crop_rect == NULL) {
        return 0;
    }
    int crop_w = crop_rect->right - crop_rect->left;
    int crop_h = crop_rect->bottom - crop_rect->top;
    if (crop_w < 120 || crop_w > 560 || crop_h < 48 || crop_h > 560) {
        return 0;
    }
    if (full_width > 0 && full_height > 0) {
        int crop_pixels = crop_w * crop_h;
        int full_pixels = full_width * full_height;
        if (crop_pixels > full_pixels / 4) {
            return 0;
        }
    }
    return 1;
}

static int kbo_profile_ocr_click_menu_top_fallback(
    HWND hwnd,
    const RECT* capture_rect,
    const RECT* crop_rect,
    int full_width,
    int full_height,
    uint32_t player_id)
{
    if (hwnd == NULL || capture_rect == NULL || crop_rect == NULL) {
        return 0;
    }
    if (read_kbo_localappdata_flag_file("disable_player_profile_ocr_menu_top_fallback.txt")) {
        return 0;
    }
    if (!kbo_profile_ocr_crop_looks_like_menu(crop_rect, full_width, full_height)) {
        append_logf("KBO player profile OCR menu top fallback skipped player=%u crop=%ld,%ld,%ld,%ld full=%dx%d",
            player_id,
            crop_rect->left,
            crop_rect->top,
            crop_rect->right,
            crop_rect->bottom,
            full_width,
            full_height);
        return 0;
    }

    int crop_w = crop_rect->right - crop_rect->left;
    int crop_h = crop_rect->bottom - crop_rect->top;
    int local_x = crop_w / 2;
    int local_y = 18;
    if (crop_h < 72) {
        local_y = crop_h / 2;
    }
    if (local_y < 8) {
        local_y = 8;
    }

    int screen_x = capture_rect->left + crop_rect->left + local_x;
    int screen_y = capture_rect->top + crop_rect->top + local_y;
    append_logf("KBO player profile OCR menu top fallback click player=%u screen=%d,%d local=%d,%d crop=%ld,%ld,%ld,%ld",
        player_id,
        screen_x,
        screen_y,
        local_x,
        local_y,
        crop_rect->left,
        crop_rect->top,
        crop_rect->right,
        crop_rect->bottom);
    return kbo_profile_ocr_send_click(hwnd, screen_x, screen_y, player_id, "menu-top-fallback");
}

static int kbo_profile_ocr_send_click(HWND hwnd, int screen_x, int screen_y, uint32_t player_id, const char* phrase)
{
    if (hwnd == NULL || !IsWindow(hwnd)) {
        append_logf("KBO player profile OCR virtual click failed: invalid hwnd player=%u hwnd=%p",
            player_id, (void*)hwnd);
        return 0;
    }

    POINT cursor_before = {0, 0};
    POINT cursor_after = {0, 0};
    GetCursorPos(&cursor_before);

    POINT client;
    client.x = screen_x;
    client.y = screen_y;
    if (!ScreenToClient(hwnd, &client)) {
        append_logf("KBO player profile OCR virtual click failed: ScreenToClient player=%u screen=%d,%d error=%lu",
            player_id, screen_x, screen_y, GetLastError());
        return 0;
    }

    RECT client_rect;
    if (!GetClientRect(hwnd, &client_rect)
            || client.x < client_rect.left || client.x >= client_rect.right
            || client.y < client_rect.top || client.y >= client_rect.bottom) {
        append_logf("KBO player profile OCR virtual click failed: client point outside player=%u screen=%d,%d client=%ld,%ld rect=%ld,%ld,%ld,%ld",
            player_id, screen_x, screen_y, client.x, client.y,
            client_rect.left, client_rect.top, client_rect.right, client_rect.bottom);
        return 0;
    }

    int hook_ready = kbo_profile_ocr_install_virtual_mouse_hooks();
    LPARAM lparam = kbo_profile_ocr_mouse_lparam((int)client.x, (int)client.y);
    kbo_profile_ocr_virtual_mouse_set(hwnd, screen_x, screen_y, 0);
    PostMessageA(hwnd, WM_ACTIVATEAPP, TRUE, GetCurrentThreadId());
    PostMessageA(hwnd, WM_NCACTIVATE, TRUE, 0);
    PostMessageA(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
    PostMessageA(hwnd, WM_SETFOCUS, 0, 0);
    PostMessageA(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    BOOL moved = PostMessageA(hwnd, WM_MOUSEMOVE, 0, lparam);
    Sleep(90);
    kbo_profile_ocr_virtual_mouse_set(hwnd, screen_x, screen_y, 1);
    PostMessageA(hwnd, WM_MOUSEACTIVATE, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
    BOOL down = PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lparam);
    Sleep(160);
    kbo_profile_ocr_virtual_mouse_set(hwnd, screen_x, screen_y, 0);
    BOOL up = PostMessageA(hwnd, WM_LBUTTONUP, 0, lparam);
    PostMessageA(hwnd, WM_MOUSEMOVE, 0, lparam);
    Sleep(450);
    kbo_profile_ocr_virtual_mouse_clear();
    GetCursorPos(&cursor_after);

    append_logf("KBO player profile OCR virtual click sent player=%u phrase=\"%s\" screen=%d,%d client=%ld,%ld hwnd=%p posted=%d/%d/%d hook=%d cursor=%ld,%ld->%ld,%ld",
        player_id,
        phrase != NULL ? phrase : "",
        screen_x,
        screen_y,
        client.x,
        client.y,
        (void*)hwnd,
        moved ? 1 : 0,
        down ? 1 : 0,
        up ? 1 : 0,
        hook_ready,
        cursor_before.x,
        cursor_before.y,
        cursor_after.x,
        cursor_after.y);
    return moved && down && up;
}

static int kbo_profile_ocr_queue_menu_action(uint32_t player_id, const char* phrase)
{
    if (player_id == 0u) {
        return 0;
    }
    if (g_kbo_game_thread_id == 0) {
        append_logf("KBO player profile OCR menu action queue failed: no game thread player=%u phrase=\"%s\"",
            player_id, phrase != NULL ? phrase : "");
        return 0;
    }

    if (!PostThreadMessageA(g_kbo_game_thread_id, KBO_WM_PLAYER_PROFILE_MENU_ACTION, (WPARAM)player_id, 0)) {
        append_logf("KBO player profile OCR menu action queue failed player=%u phrase=\"%s\" thread=%lu error=%lu",
            player_id,
            phrase != NULL ? phrase : "",
            (unsigned long)g_kbo_game_thread_id,
            GetLastError());
        return 0;
    }

    append_logf("KBO player profile OCR menu action queued player=%u phrase=\"%s\" thread=%lu",
        player_id, phrase != NULL ? phrase : "", (unsigned long)g_kbo_game_thread_id);
    return 1;
}

static DWORD WINAPI kbo_player_profile_ocr_click_thread(LPVOID param)
{
    KboPlayerProfileOcrClickJob* job = (KboPlayerProfileOcrClickJob*)param;
    if (job == NULL) {
        InterlockedExchange(&g_kbo_player_profile_ocr_click_v1_active, 0);
        return 0;
    }

    HWND hwnd = job->hwnd;
    uint32_t player_id = job->player_id;
    RECT capture_rect = job->capture_rect;
    int popup_opened = job->popup_opened;
    unsigned char* before_pixels = job->before_pixels;
    int before_width = job->before_width;
    int before_height = job->before_height;
    HeapFree(GetProcessHeap(), 0, job);

    ULONGLONG started = GetTickCount64();
    Sleep(160);

    char tesseract[MAX_PATH] = {0};
    char bmp_path[MAX_PATH] = {0};
    char out_base[MAX_PATH] = {0};
    char tsv_path[MAX_PATH] = {0};
    int clicked = 0;
    unsigned char* after_pixels = NULL;
    RECT crop_rect = {0, 0, 0, 0};
    int crop_from_diff = 0;
    int ocr_scale = 1;
    int width = before_width;
    int height = before_height;

    if (!popup_opened) {
        append_logf("KBO player profile OCR click worker continuing after popup_opened=0 player=%u",
            player_id);
    }

    if (!kbo_profile_ocr_find_tesseract(tesseract, sizeof(tesseract))) {
        append_logf("KBO player profile OCR click failed: tesseract not found player=%u", player_id);
        goto cleanup;
    }
    if (!kbo_profile_ocr_build_temp_paths(player_id, bmp_path, sizeof(bmp_path), out_base, sizeof(out_base), tsv_path, sizeof(tsv_path))) {
        append_logf("KBO player profile OCR click failed: temp paths player=%u", player_id);
        goto cleanup;
    }

    if (width <= 16 || height <= 16) {
        width = capture_rect.right - capture_rect.left;
        height = capture_rect.bottom - capture_rect.top;
    }
    after_pixels = kbo_profile_ocr_capture_window_pixels(hwnd, width, height);
    if (after_pixels == NULL) {
        goto cleanup;
    }
    if (before_pixels != NULL
            && before_width == width
            && before_height == height
            && kbo_profile_ocr_find_changed_rect(before_pixels, after_pixels, width, height, &crop_rect)) {
        crop_from_diff = 1;
    } else {
        crop_rect.left = 0;
        crop_rect.top = 0;
        crop_rect.right = width;
        crop_rect.bottom = height;
    }
    if (!kbo_profile_ocr_save_bmp_region(after_pixels, width, height, &crop_rect, bmp_path, &ocr_scale)) {
        goto cleanup;
    }

    ULONGLONG before_tesseract = GetTickCount64();
    if (!kbo_profile_ocr_run_tesseract(tesseract, bmp_path, out_base, 3000)) {
        goto cleanup;
    }
    ULONGLONG after_tesseract = GetTickCount64();

    POINT image_point;
    char phrase[64] = {0};
    int target_result = kbo_profile_ocr_find_target_in_tsv(tsv_path, player_id, &image_point, phrase, sizeof(phrase));
    if (target_result > 0) {
        int local_x = (int)image_point.x;
        int local_y = (int)image_point.y;
        if (ocr_scale > 1) {
            local_x = (local_x + ocr_scale / 2) / ocr_scale;
            local_y = (local_y + ocr_scale / 2) / ocr_scale;
        }
        int screen_x = capture_rect.left + crop_rect.left + local_x;
        int screen_y = capture_rect.top + crop_rect.top + local_y;
        append_logf("KBO player profile OCR target mapped player=%u phrase=\"%s\" ocr=%ld,%ld scale=%d local=%d,%d screen=%d,%d",
            player_id, phrase, image_point.x, image_point.y, ocr_scale, local_x, local_y, screen_x, screen_y);
        clicked = kbo_profile_ocr_queue_menu_action(player_id, phrase);
        if (!clicked) {
            clicked = kbo_profile_ocr_send_click(hwnd, screen_x, screen_y, player_id, phrase);
        }
    } else if (target_result < 0) {
        append_logf("KBO player profile OCR click aborted: blocking modal visible player=%u path=%s",
            player_id, tsv_path);
    } else if (crop_from_diff) {
        clicked = kbo_profile_ocr_click_menu_top_fallback(hwnd, &capture_rect, &crop_rect, width, height, player_id);
    }
    append_logf("KBO player profile OCR timing player=%u crop_diff=%d crop=%ld,%ld,%ld,%ld capture_to_ocr_ms=%llu tesseract_ms=%llu total_ms=%llu",
        player_id,
        crop_from_diff,
        crop_rect.left,
        crop_rect.top,
        crop_rect.right,
        crop_rect.bottom,
        (unsigned long long)(before_tesseract - started),
        (unsigned long long)(after_tesseract - before_tesseract),
        (unsigned long long)(GetTickCount64() - started));

cleanup:
    if (after_pixels != NULL) {
        HeapFree(GetProcessHeap(), 0, after_pixels);
    }
    if (before_pixels != NULL) {
        HeapFree(GetProcessHeap(), 0, before_pixels);
    }
    if (!read_kbo_localappdata_flag_file("enable_player_profile_ocr_keep_artifacts.txt")) {
        if (bmp_path[0] != '\0') {
            DeleteFileA(bmp_path);
        }
        if (tsv_path[0] != '\0') {
            DeleteFileA(tsv_path);
        }
    } else {
        append_logf("KBO player profile OCR artifacts kept player=%u bmp=%s tsv=%s",
            player_id, bmp_path, tsv_path);
    }

    append_logf("KBO player profile OCR click worker done player=%u clicked=%d", player_id, clicked);
    InterlockedExchange(&g_kbo_player_profile_ocr_click_v1_active, 0);
    return clicked ? 1u : 0u;
}

static int kbo_try_player_profile_ocr_click_v1(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL || player_id == 0u) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_kbo_player_profile_ocr_click_v1_last_ms < 1400u) {
        append_logf("KBO player profile OCR click v1 skipped: cooldown player=%u elapsed_ms=%llu",
            player_id, (unsigned long long)(now - g_kbo_player_profile_ocr_click_v1_last_ms));
        return 0;
    }
    g_kbo_player_profile_ocr_click_v1_last_ms = now;

    if (InterlockedCompareExchange(&g_kbo_player_profile_ocr_click_v1_active, 1, 0) != 0) {
        append_logf("KBO player profile OCR click v1 skipped: worker active player=%u", player_id);
        return 0;
    }

    HWND hwnd = kbo_find_ootp_main_window();
    if (hwnd == NULL || !IsWindow(hwnd) || IsIconic(hwnd)) {
        append_logf("KBO player profile OCR click v1 skipped: invalid OOTP hwnd player=%u hwnd=%p",
            player_id, (void*)hwnd);
        InterlockedExchange(&g_kbo_player_profile_ocr_click_v1_active, 0);
        return 0;
    }

    RECT capture_rect;
    if (!GetWindowRect(hwnd, &capture_rect)) {
        append_logf("KBO player profile OCR click v1 skipped: GetWindowRect player=%u error=%lu",
            player_id, GetLastError());
        InterlockedExchange(&g_kbo_player_profile_ocr_click_v1_active, 0);
        return 0;
    }

    int width = capture_rect.right - capture_rect.left;
    int height = capture_rect.bottom - capture_rect.top;
    if (width <= 16 || height <= 16) {
        append_logf("KBO player profile OCR click v1 skipped: invalid window size player=%u rect=%ld,%ld,%ld,%ld",
            player_id, capture_rect.left, capture_rect.top, capture_rect.right, capture_rect.bottom);
        InterlockedExchange(&g_kbo_player_profile_ocr_click_v1_active, 0);
        return 0;
    }

    ULONGLONG before_capture_start = GetTickCount64();
    unsigned char* before_pixels = kbo_profile_ocr_capture_window_pixels(hwnd, width, height);
    append_logf("KBO player profile OCR click v1 before capture player=%u ok=%d ms=%llu size=%dx%d",
        player_id,
        before_pixels != NULL ? 1 : 0,
        (unsigned long long)(GetTickCount64() - before_capture_start),
        width,
        height);

    int popup_opened = kbo_try_player_profile_global_open_v7(exe, player_id);
    KboPlayerProfileOcrClickJob* job = (KboPlayerProfileOcrClickJob*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(KboPlayerProfileOcrClickJob));
    if (job == NULL) {
        append_logf("KBO player profile OCR click v1 failed: job allocation player=%u", player_id);
        if (before_pixels != NULL) {
            HeapFree(GetProcessHeap(), 0, before_pixels);
        }
        InterlockedExchange(&g_kbo_player_profile_ocr_click_v1_active, 0);
        return popup_opened;
    }
    job->hwnd = hwnd;
    job->capture_rect = capture_rect;
    job->player_id = player_id;
    job->popup_opened = popup_opened;
    job->before_pixels = before_pixels;
    job->before_width = width;
    job->before_height = height;

    HANDLE thread = CreateThread(NULL, 0, kbo_player_profile_ocr_click_thread, job, 0, NULL);
    if (thread == NULL) {
        append_logf("KBO player profile OCR click v1 failed: CreateThread player=%u error=%lu",
            player_id, GetLastError());
        if (before_pixels != NULL) {
            HeapFree(GetProcessHeap(), 0, before_pixels);
        }
        HeapFree(GetProcessHeap(), 0, job);
        InterlockedExchange(&g_kbo_player_profile_ocr_click_v1_active, 0);
        return popup_opened;
    }
    CloseHandle(thread);
    append_logf("KBO player profile OCR click v1 queued player=%u popup_opened=%d rect=%ld,%ld,%ld,%ld",
        player_id, popup_opened, capture_rect.left, capture_rect.top, capture_rect.right, capture_rect.bottom);
    return 1;
}

static void kbo_log_ootp_player_profile_probe(HMODULE exe, uint32_t player_id)
{
    if (exe == NULL) {
        return;
    }

    uintptr_t ui_state = (uintptr_t)exe + OOTP27_UI_CONTEXT_ROOT_RVA - 0x1610u;
    OotpUiCurrentModeFn current_mode = (OotpUiCurrentModeFn)((uintptr_t)exe + OOTP27_UI_CURRENT_MODE_RVA);
    OotpUiCurrentPageFn current_page = (OotpUiCurrentPageFn)((uintptr_t)exe + OOTP27_UI_CURRENT_PAGE_RVA);
    int mode0 = -1;
    int mode3 = -1;
    void* page0 = NULL;
    void* page3 = NULL;

    if (memory_range_readable((void*)ui_state, 0x40u)) {
        mode0 = current_mode((void*)ui_state, 0);
        mode3 = current_mode((void*)ui_state, 3);
        page0 = current_page((void*)ui_state, 0);
        page3 = current_page((void*)ui_state, 3);
    }

    void* context = kbo_get_ootp_current_ui_context(exe);
    kbo_log_ootp_ui_slot_candidates(exe);
    kbo_log_ootp_page_callback_candidates("current page", context);
    append_logf("KBO player profile probe: player=%u ui_state=%p mode0=%d page0=%p mode3=%d page3=%p context=%p menu_callback=-0x3ed global_open_callback=-0x3ee old_callback=-0x3f2",
        player_id, (void*)ui_state, mode0, page0, mode3, page3, context);
}

static int kbo_open_ootp_player_profile_now(uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return 0;
    }

    int flag_ocr = read_kbo_localappdata_flag_file("enable_player_profile_ocr_click_v1.txt");
    if (!flag_ocr || read_kbo_localappdata_flag_file("enable_player_profile_ocr_probe_log.txt")) {
        kbo_log_ootp_player_profile_probe(exe, player_id);
    }
    int flag_v25 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v25.txt");
    int flag_v24 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v24.txt");
    int flag_v23 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v23.txt");
    int flag_v22 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v22.txt");
    int flag_v21 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v21.txt");
    int flag_v20 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v20.txt");
    int flag_v19 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v19.txt");
    int flag_v18 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v18.txt");
    int flag_v17 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v17.txt");
    int flag_v16 = read_kbo_localappdata_flag_file("enable_player_profile_global_open_v16.txt");
    int flag_direct_v26 = read_kbo_localappdata_flag_file("enable_player_profile_direct_install_v26.txt");
    append_logf("KBO player profile flags player=%u ocr=%d direct_v26=%d v25=%d v24=%d v23=%d v22=%d v21=%d v20=%d v19=%d v18=%d v17=%d v16=%d",
        player_id, flag_ocr, flag_direct_v26, flag_v25, flag_v24, flag_v23, flag_v22, flag_v21, flag_v20, flag_v19, flag_v18, flag_v17, flag_v16);
    if (flag_ocr) {
        return kbo_try_player_profile_ocr_click_v1(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_page_callback_v4.txt")) {
        return kbo_try_player_profile_page_callback_v4(exe, player_id);
    }

    if (flag_v25) {
        return kbo_try_player_profile_global_open_v25(exe, player_id);
    }

    if (flag_v24) {
        return kbo_try_player_profile_global_open_v24(exe, player_id);
    }

    if (flag_v23) {
        return kbo_try_player_profile_global_open_v23(exe, player_id);
    }

    if (flag_v22) {
        return kbo_try_player_profile_global_open_v22(exe, player_id);
    }

    if (flag_v21) {
        return kbo_try_player_profile_global_open_v21(exe, player_id);
    }

    if (flag_v20) {
        return kbo_try_player_profile_global_open_v20(exe, player_id);
    }

    if (flag_v19) {
        return kbo_try_player_profile_global_open_v19(exe, player_id);
    }

    if (flag_v18) {
        return kbo_try_player_profile_global_open_v18(exe, player_id);
    }

    if (flag_v17) {
        return kbo_try_player_profile_global_open_v17(exe, player_id);
    }

    if (flag_v16) {
        return kbo_try_player_profile_global_open_v16(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v15.txt")) {
        return kbo_try_player_profile_global_open_v15(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v14.txt")) {
        return kbo_try_player_profile_global_open_v14(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v13.txt")) {
        return kbo_try_player_profile_global_open_v13(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v12.txt")) {
        return kbo_try_player_profile_global_open_v12(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v11.txt")) {
        return kbo_try_player_profile_global_open_v11(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v10.txt")) {
        return kbo_try_player_profile_global_open_v10(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v9.txt")) {
        return kbo_try_player_profile_global_open_v9(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v8.txt")) {
        return kbo_try_player_profile_global_open_v8(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v7.txt")) {
        return kbo_try_player_profile_global_open_v7(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v6.txt")) {
        return kbo_try_player_profile_global_open_v6(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_global_open_v5.txt")) {
        return kbo_try_player_profile_global_open_v5(exe, player_id);
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_direct_open_v3.txt")) {
        append_logf("KBO player profile direct open v3 disabled: open_page returned but broke OOTP input player=%u",
            player_id);
        return 0;
    }

    if (read_kbo_localappdata_flag_file("enable_player_profile_direct_open_v2.txt")) {
        append_logf("KBO player profile direct open v2 disabled: opener candidate was invalid player=%u",
            player_id);
        return 0;
    }

    append_logf("KBO player profile open disabled: direct page install and raw callback dispatch are unsafe player=%u",
        player_id);
    return 0;
}

static int kbo_open_ootp_player_profile_async(uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }

    kbo_try_install_game_getmsg_hook();
    if (g_kbo_game_thread_id == 0) {
        HWND game_hwnd = kbo_find_ootp_main_window();
        if (game_hwnd != NULL) {
            g_kbo_game_thread_id = GetWindowThreadProcessId(game_hwnd, NULL);
        }
    }

    if (g_kbo_game_thread_id == 0) {
        append_logf("KBO player profile open failed: no game thread player=%u", player_id);
        return 0;
    }

    if (!PostThreadMessageA(g_kbo_game_thread_id, KBO_WM_OPEN_PLAYER_PROFILE, (WPARAM)player_id, 0)) {
        append_logf("KBO player profile open failed: PostThreadMessage player=%u thread=%lu error=%lu",
            player_id, (unsigned long)g_kbo_game_thread_id, GetLastError());
        return 0;
    }

    append_logf("KBO player profile open queued player=%u thread=%lu",
        player_id, (unsigned long)g_kbo_game_thread_id);
    return 1;
}

static LRESULT CALLBACK kbo_hotkey_keyboard_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION && (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) && lparam != 0) {
        const KBDLLHOOKSTRUCT* key = (const KBDLLHOOKSTRUCT*)lparam;
        if (key->vkCode == VK_F2 && kbo_foreground_is_this_process()) {
            kbo_queue_hotkey_window_toggle();
            return 1;
        }
        if (key->vkCode == VK_F5
                && g_kbo_hotkey_window != NULL
                && IsWindowVisible(g_kbo_hotkey_window)
                && kbo_foreground_is_this_process()) {
            kbo_refresh_hotkey_window();
            InvalidateRect(g_kbo_hotkey_window, NULL, TRUE);
            return 1;
        }
    }

    return CallNextHookEx(g_kbo_hotkey_keyboard_hook, code, wparam, lparam);
}

static void kbo_hub_draw_text(HDC hdc, const char* text, RECT rect, COLORREF color, HFONT font, UINT format)
{
    if (text == NULL) {
        return;
    }

    HFONT old_font = NULL;
    if (font != NULL) {
        old_font = (HFONT)SelectObject(hdc, font);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len > 0) {
        WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len) > 0) {
                DrawTextW(hdc, wide, -1, &rect, format);
            }
            HeapFree(GetProcessHeap(), 0, wide);
        }
    }

    if (old_font != NULL) {
        SelectObject(hdc, old_font);
    }
}

static int kbo_hub_ascii_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int kbo_hub_ascii_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int kbo_hub_ascii_is_alnum(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int kbo_hub_name_fragment_plausible(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    size_t len = strlen(text);
    if (len < 2 || len > 32) {
        return 0;
    }

    int alpha_count = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (kbo_hub_ascii_is_digit(c)) {
            return 0;
        }
        if (c == '_' || c == '/' || c == '\\' || c == ':' || c == '%' || c == '#' || c == '$' || c == '@') {
            return 0;
        }
        if (!(kbo_hub_ascii_is_alpha(c) || c == ' ' || c == '-' || c == '\'' || c == '.')) {
            return 0;
        }
        if (kbo_hub_ascii_is_alpha(c)) {
            alpha_count++;
        }
    }

    if (alpha_count < 2) {
        return 0;
    }
    if (ascii_equals_ignore_case(text, "KBO")
            || ascii_equals_ignore_case(text, "MLB")
            || ascii_equals_ignore_case(text, "SANG")
            || ascii_equals_ignore_case(text, "KPB")
            || ascii_equals_ignore_case(text, "USA")) {
        return 0;
    }

    return 1;
}

static int kbo_hub_copy_raw_pointer_string(uint8_t* object_base, uint32_t pointer_offset, char* out, size_t out_size)
{
    if (object_base == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    uint8_t* pointer_slot = object_base + pointer_offset;
    if (!memory_range_readable(pointer_slot, sizeof(char*))) {
        return 0;
    }
    const char* text = *(const char**)pointer_slot;
    return copy_limited_ascii_string(text, out, out_size);
}

static int kbo_hub_append_unique_name_fragment(char fragments[][48], int* count, const char* text)
{
    if (fragments == NULL || count == NULL || *count >= 3 || !kbo_hub_name_fragment_plausible(text)) {
        return 0;
    }
    for (int i = 0; i < *count; i++) {
        if (ascii_equals_ignore_case(fragments[i], text)) {
            return 0;
        }
    }
    snprintf(fragments[*count], 48, "%s", text);
    (*count)++;
    return 1;
}

static void kbo_hub_copy_player_display_name(uint8_t* player, char* out, size_t out_size)
{
    kbo_copy_player_display_name(player, out, out_size);
}

static int kbo_hub_team_name_candidate_score(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return -1000;
    }

    size_t len = strlen(text);
    if (len < 2 || len > 64) {
        return -1000;
    }

    int alpha_count = 0;
    int space_count = 0;
    int lowercase_count = 0;
    int uppercase_count = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c < 0x20 || c > 0x7e || kbo_hub_ascii_is_digit(c)) {
            return -1000;
        }
        if (kbo_hub_ascii_is_alpha(c)) {
            alpha_count++;
            if (c >= 'a' && c <= 'z') { lowercase_count++; } else { uppercase_count++; }
        } else if (c == ' ') {
            space_count++;
        } else if (!(c == '-' || c == '\'' || c == '.' || c == '&')) {
            return -1000;
        }
    }

    if (alpha_count < 2) {
        return -1000;
    }

    int score = (int)len;
    if (space_count > 0) { score += 80; }
    if (lowercase_count > 0) { score += 20; }
    if (space_count == 0 && len <= 5 && uppercase_count == alpha_count) { score -= 80; }
    if (ascii_equals_ignore_case(text, "SANG") || ascii_equals_ignore_case(text, "KPB")) { score -= 200; }

    return score;
}

static void kbo_hub_copy_team_display_name_from_ptr(uint8_t* team, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        snprintf(out, out_size, "%s", fallback != NULL && fallback[0] != '\0' ? fallback : "Unknown club");
        return;
    }

    static const uint32_t string_offsets[] = { 0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u };

    char city[64] = {0};
    char nickname[64] = {0};
    char best[96] = {0};
    int best_score = -1000;

    copy_ootp_string_object_text(team, 0x10u, city,     sizeof(city));
    copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));

    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char text[96] = {0};
        if (!copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))) {
            continue;
        }
        int score = kbo_hub_team_name_candidate_score(text);
        if (score > best_score) {
            best_score = score;
            snprintf(best, sizeof(best), "%s", text);
        }
    }

    if (best_score > 0 && strchr(best, ' ') != NULL) {
        snprintf(out, out_size, "%s", best);
        return;
    }

    if (kbo_hub_team_name_candidate_score(city) > 0
            && kbo_hub_team_name_candidate_score(nickname) > 0
            && !ascii_equals_ignore_case(city, nickname)) {
        snprintf(out, out_size, "%s %s", city, nickname);
        return;
    }

    if (best_score > 0) {
        snprintf(out, out_size, "%s", best);
        return;
    }

    snprintf(out, out_size, "%s", fallback != NULL && fallback[0] != '\0' ? fallback : "Unknown club");
}

static void kbo_hub_copy_team_display_name_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL) {
        kbo_hub_copy_team_display_name_from_ptr(team, out, out_size, fallback);
        return;
    }

    if (fallback != NULL && fallback[0] != '\0') {
        snprintf(out, out_size, "%s", fallback);
    } else if (team_id != 0) {
        snprintf(out, out_size, "Unknown club #%u", team_id);
    } else {
        snprintf(out, out_size, "Unknown club");
    }
}

static void kbo_hub_copy_team_abbrev_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint32_t parent_team_id = 0u;
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    }
    if (parent_team_id != 0u && parent_team_id != team_id) {
        char parent_abbrev[16] = {0};
        kbo_hub_copy_team_abbrev_by_id(parent_team_id, parent_abbrev, sizeof(parent_abbrev), fallback);
        if (parent_abbrev[0] != '\0' && strcmp(parent_abbrev, "-") != 0) {
            snprintf(out, out_size, "%s2", parent_abbrev);
            return;
        }
    }

    char name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, name, sizeof(name), fallback);
    if (kbo_ascii_contains_ignore_case(name, "Lotte"))      { snprintf(out, out_size, "LOT");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Doosan"))     { snprintf(out, out_size, "DOO");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Samsung"))    { snprintf(out, out_size, "SAM");  return; }
    if (kbo_ascii_contains_ignore_case(name, "KIA"))        { snprintf(out, out_size, "KIA");  return; }
    if (kbo_ascii_contains_ignore_case(name, "SSG"))        { snprintf(out, out_size, "SSG");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Hanwha"))     { snprintf(out, out_size, "HAN");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Kiwoom"))     { snprintf(out, out_size, "KIW");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Nexen"))      { snprintf(out, out_size, "NEX");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Heroes"))     { snprintf(out, out_size, "KIW");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Dinos"))      { snprintf(out, out_size, "NC");   return; }
    if (kbo_ascii_contains_ignore_case(name, "NC"))         { snprintf(out, out_size, "NC");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Wiz"))        { snprintf(out, out_size, "KT");   return; }
    if (kbo_ascii_contains_ignore_case(name, "KT"))         { snprintf(out, out_size, "KT");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Twins"))      { snprintf(out, out_size, "LG");   return; }
    if (kbo_ascii_contains_ignore_case(name, "LG"))         { snprintf(out, out_size, "LG");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Sangmu"))     { snprintf(out, out_size, "SANG"); return; }
    if (kbo_ascii_contains_ignore_case(name, "Police"))     { snprintf(out, out_size, "KPB");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Ulsan"))      { snprintf(out, out_size, "ULS");  return; }

    if (fallback != NULL && fallback[0] != '\0' && strlen(fallback) <= 6u) {
        snprintf(out, out_size, "%s", fallback);
    } else if (team_id != 0u) {
        snprintf(out, out_size, "T%u", team_id);
    } else {
        snprintf(out, out_size, "-");
    }
}

static void kbo_hub_sanitize_logo_prefix(const char* name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (name == NULL) {
        return;
    }

    size_t write = 0;
    int last_was_sep = 1;
    for (size_t read = 0; name[read] != '\0' && write + 1 < out_size; read++) {
        char ch = name[read];
        if (kbo_hub_ascii_is_alnum(ch)) {
            out[write++] = ch;
            last_was_sep = 0;
        } else if (!last_was_sep) {
            out[write++] = '_';
            last_was_sep = 1;
        }
    }
    while (write > 0 && out[write - 1] == '_') {
        write--;
    }
    out[write] = '\0';
}

static int kbo_hub_parse_four_digits(const char* text, int index)
{
    int value = 0;
    for (int i = 0; i < 4; i++) {
        char ch = text[index + i];
        if (ch < '0' || ch > '9') {
            return -1;
        }
        value = (value * 10) + (ch - '0');
    }
    return value;
}

static int kbo_hub_logo_filename_score(const char* file_name, uint32_t year)
{
    if (file_name == NULL || file_name[0] == '\0') {
        return -1000000;
    }

    int has_year_token = 0;
    int best_year_score = year == 0 ? 0 : -100000;
    size_t len = strlen(file_name);
    for (size_t i = 0; i + 4 <= len; i++) {
        int start_year = kbo_hub_parse_four_digits(file_name, (int)i);
        if (start_year < 1800 || start_year > 3000) {
            continue;
        }
        has_year_token = 1;
        int score = -5000;
        if (i + 9 <= len && file_name[i + 4] == '-') {
            int end_year = kbo_hub_parse_four_digits(file_name, (int)i + 5);
            if (end_year >= start_year) {
                score = (year >= (uint32_t)start_year && year <= (uint32_t)end_year)
                    ? 10000 - abs((int)year - start_year) : -1000;
                i += 8;
            }
        } else if (year == (uint32_t)start_year) {
            score = 12000;
        }
        if (score > best_year_score) {
            best_year_score = score;
        }
    }

    int score = has_year_token ? best_year_score : 100;
    if (strstr(file_name, "_small_50") != NULL) { score += 700; }
    if (strstr(file_name, "_away")     != NULL
            || strstr(file_name, "_Away") != NULL) { score -= 500; }
    return score;
}

static int kbo_hub_find_best_logo_file(
    const char* directory, const char* search_prefix,
    uint32_t year, char* out, size_t out_size)
{
    if (directory == NULL || search_prefix == NULL || search_prefix[0] == '\0'
            || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\%s*.*", directory, search_prefix);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int best_score = -1000000;
    char best_path[MAX_PATH] = {0};
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        size_t name_len = strlen(find_data.cFileName);
        int is_png = name_len >= 4 && ascii_equals_ignore_case(find_data.cFileName + name_len - 4, ".png");
        int is_oi  = name_len >= 3 && ascii_equals_ignore_case(find_data.cFileName + name_len - 3, ".oi");
        if (!is_png && !is_oi) {
            continue;
        }
        int score = kbo_hub_logo_filename_score(find_data.cFileName, year);
        if (is_png) { score += 50; }
        if (score > best_score) {
            best_score = score;
            snprintf(best_path, sizeof(best_path), "%s\\%s", directory, find_data.cFileName);
        }
    } while (FindNextFileA(find, &find_data));

    FindClose(find);
    if (best_path[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s", best_path);
    return 1;
}

static int kbo_hub_get_league_logo_path(uint32_t league_id, uint32_t year, char* out, size_t out_size)
{
    (void)league_id;
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    const char* league_logo = "2024~ KBO.png";
    if      (year >= 1982 && year <= 1999) { league_logo = "1982~1999 KBO.png"; }
    else if (year >= 2000 && year <= 2011) { league_logo = "2000~2011 KBO.png"; }
    else if (year >= 2012 && year <= 2023) { league_logo = "2012~2023 KBO.png"; }

    kbo_hub_logo_asset_path(league_logo, out, out_size);
    return out[0] != '\0' && GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES;
}

static int kbo_hub_get_team_logo_path(uint32_t team_id, uint32_t year, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    char prefix[96] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, team_name, sizeof(team_name), NULL);
    kbo_hub_sanitize_logo_prefix(team_name, prefix, sizeof(prefix));
    if (prefix[0] == '\0') {
        return 0;
    }

    if (kbo_hub_find_best_logo_file(
            "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\logos",
            prefix, year, out, out_size)) {
        return 1;
    }

    char cap_prefix[112] = {0};
    snprintf(cap_prefix, sizeof(cap_prefix), "caps_%s", prefix);
    return kbo_hub_find_best_logo_file(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\ballcaps",
        cap_prefix, year, out, out_size);
}

static void kbo_hub_ensure_header_logos(uint32_t league_id, uint32_t team_id, uint32_t year)
{
    if (g_kbo_hub_logo_cache_league_id == league_id
            && g_kbo_hub_logo_cache_team_id == team_id
            && g_kbo_hub_logo_cache_year == year) {
        return;
    }

    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_league_logo);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_team_logo);
    g_kbo_hub_logo_cache_league_id = league_id;
    g_kbo_hub_logo_cache_team_id   = team_id;
    g_kbo_hub_logo_cache_year      = year;

    char path[MAX_PATH] = {0};
    kbo_hub_get_league_logo_path(league_id, year, path, sizeof(path));
    g_kbo_hub_asset_selected_league_logo = kbo_hub_load_png_hbitmap_wic(path);

    if (kbo_hub_get_team_logo_path(team_id, year, path, sizeof(path))) {
        g_kbo_hub_asset_selected_team_logo = kbo_hub_load_png_hbitmap_wic(path);
    }
}

static LRESULT CALLBACK kbo_game_getmsg_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION && lparam != 0 && wparam == PM_REMOVE) {
        MSG* msg = (MSG*)lparam;
        kbo_profile_ocr_rewrite_mouse_msg(msg, "getmsg");
        if (msg->message == KBO_WM_OPEN_PLAYER_PROFILE) {
            uint32_t player_id = (uint32_t)msg->wParam;
            kbo_open_ootp_player_profile_now(player_id);
            msg->message = WM_NULL;
            msg->wParam = 0;
            msg->lParam = 0;
            return 1;
        }
        if (msg->message == KBO_WM_PLAYER_PROFILE_MENU_ACTION) {
            uint32_t player_id = (uint32_t)msg->wParam;
            HMODULE exe = GetModuleHandleA(NULL);
            kbo_try_player_profile_menu_action_now(exe, player_id, "ocr-menu-action");
            msg->message = WM_NULL;
            msg->wParam = 0;
            msg->lParam = 0;
            return 1;
        }
    }
    return CallNextHookEx(g_kbo_game_getmsg_hook, code, wparam, lparam);
}

static void kbo_try_install_game_getmsg_hook(void)
{
    if (g_kbo_game_getmsg_hook != NULL) {
        return;
    }
    HWND game_hwnd = kbo_find_ootp_main_window();
    if (game_hwnd == NULL) {
        return;
    }
    DWORD game_thread_id = GetWindowThreadProcessId(game_hwnd, NULL);
    if (game_thread_id == 0) {
        return;
    }
    g_kbo_game_thread_id = game_thread_id;
    g_kbo_game_getmsg_hook = SetWindowsHookExA(
        WH_GETMESSAGE, kbo_game_getmsg_hook_proc, NULL, game_thread_id);
    if (g_kbo_game_getmsg_hook != NULL) {
        append_logf("KBO game getmsg hook installed thread=%lu", (unsigned long)game_thread_id);
    } else {
        append_logf("KBO game getmsg hook failed error=%lu", GetLastError());
    }
}
