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

