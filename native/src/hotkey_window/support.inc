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
typedef void (__fastcall *OotpPlayerPageCtorFn)(void* page, uint32_t player_id, int show_history, int add_to_history);
typedef void (__fastcall *OotpUiOpenPageFn)(void* ui_context, void* page, int flags);
typedef int (__fastcall *OotpUiCurrentModeFn)(void* ui_state, int index);
typedef void* (__fastcall *OotpUiCurrentPageFn)(void* ui_state, int index);

static void* kbo_get_ootp_current_ui_context(HMODULE exe)
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

    uintptr_t context_slot = slot + ((uintptr_t)(uint32_t)sub_index * 0x40u) + 0x38u;
    if (!memory_range_readable((void*)context_slot, sizeof(uintptr_t))) {
        append_logf("KBO player profile ui context probe: context slot unreadable slot=%p sub=%d addr=%p",
            (void*)slot, (int)sub_index, (void*)context_slot);
        return NULL;
    }

    uintptr_t context = *(uintptr_t*)context_slot;
    if (context == 0 || !memory_range_readable((void*)context, 0x18u)) {
        append_logf("KBO player profile ui context probe: context invalid root=%p index=%d sub=%d context=%p",
            (void*)root, (int)index, (int)sub_index, (void*)context);
        return NULL;
    }

    append_logf("KBO player profile ui context probe: root=%p index=%d sub=%d context=%p",
        (void*)root, (int)index, (int)sub_index, (void*)context);
    return (void*)context;
}

static int kbo_open_ootp_player_profile_direct(HMODULE exe, uint32_t player_id)
{
    void* context = kbo_get_ootp_current_ui_context(exe);
    if (context == NULL) {
        return 0;
    }

    OotpOperatorNewFn ootp_new = (OotpOperatorNewFn)((uintptr_t)exe + OOTP27_UI_OPERATOR_NEW_RVA);
    OotpPlayerPageCtorFn player_page_ctor =
        (OotpPlayerPageCtorFn)((uintptr_t)exe + OOTP27_PLAYER_PAGE_CTOR_RVA);
    OotpUiOpenPageFn open_page = (OotpUiOpenPageFn)((uintptr_t)exe + OOTP27_UI_OPEN_PAGE_RVA);

    void* page = ootp_new(0x1a8u);
    if (page == NULL) {
        append_logf("KBO player profile direct open failed: allocation player=%u", player_id);
        return 0;
    }

    append_logf("KBO player profile direct open: constructing player=%u page=%p context=%p",
        player_id, page, context);
    player_page_ctor(page, player_id, 1, 1);
    append_logf("KBO player profile direct open: installing player=%u page=%p context=%p",
        player_id, page, context);
    open_page(context, page, 0);
    append_logf("KBO player profile direct open: installed player=%u page=%p context=%p",
        player_id, page, context);
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
    append_logf("KBO player profile probe: player=%u ui_state=%p mode0=%d page0=%p mode3=%d page3=%p context=%p normal_callback=-0x3ed old_callback=-0x3f2",
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

    append_logf("KBO player profile direct open disabled: unsafe after crash player=%u", player_id);
    kbo_log_ootp_player_profile_probe(exe, player_id);

    OotpGlobalHandleCallbackFn handle_callback =
        (OotpGlobalHandleCallbackFn)((uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA);
    append_logf("KBO player profile open: direct path unavailable, calling menu callback player=%u rva=0x%x callback=%d",
        player_id, OOTP27_GLOBAL_HANDLE_CALLBACK_RVA, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU);
    int result = handle_callback(NULL, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE_MENU, (uintptr_t)player_id);
    append_logf("KBO player profile open: menu callback result=%d player=%u", result, player_id);
    if (result == 0) {
        append_logf("KBO player profile open: falling back to legacy callback player=%u callback=%d",
            player_id, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE);
        result = handle_callback(NULL, KBO_OOTP_CALLBACK_OPEN_PLAYER_PROFILE, (uintptr_t)player_id);
        append_logf("KBO player profile open: legacy callback result=%d player=%u", result, player_id);
    }
    return result != 0;
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
        if (msg->message == KBO_WM_OPEN_PLAYER_PROFILE) {
            uint32_t player_id = (uint32_t)msg->wParam;
            kbo_open_ootp_player_profile_now(player_id);
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
