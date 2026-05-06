
static void kbo_layout_hotkey_window(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    kbo_start_webview_rights_ui(hwnd);
    kbo_webview_set_bounds(hwnd);
}

static void kbo_refresh_hotkey_window_layout(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    kbo_layout_hotkey_window(hwnd);
    kbo_refresh_hotkey_window();
    if (InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) != 0) {
        kbo_webview_navigate_current();
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static RECT kbo_hub_fixed_window_rect(void)
{
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD ex_style = WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE;
    RECT rect = {0, 0, KBO_HUB_FIXED_CLIENT_WIDTH, KBO_HUB_FIXED_CLIENT_HEIGHT};
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    return rect;
}

static void kbo_hub_apply_fixed_window_size(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    RECT rect = kbo_hub_fixed_window_rect();
    SetWindowPos(
        hwnd,
        NULL,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static LRESULT CALLBACK kbo_hotkey_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        kbo_hub_load_language_setting();
        kbo_hub_init_gdi_objects();
        kbo_hub_ensure_valid_selection();
        SetTimer(hwnd, 1u, 80u, NULL);
        kbo_hub_apply_fixed_window_size(hwnd);
        kbo_layout_hotkey_window(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        kbo_layout_hotkey_window(hwnd);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc != NULL) {
            RECT client;
            GetClientRect(hwnd, &client);
            FillRect(hdc, &client, g_kbo_hub_brush_bg);
            if (InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) == 0) {
                RECT text_rect = {22, 22, client.right - 22, 70};
                kbo_hub_draw_text(hdc, "KBO FRONT OFFICE HTML UI LOADING...", text_rect,
                    RGB(245, 241, 231), g_kbo_hub_font_title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            kbo_hub_apply_fixed_window_size(hwnd);
        }
        kbo_refresh_hotkey_window_layout(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = (MINMAXINFO*)lparam;
        if (info != NULL) {
            RECT rect = kbo_hub_fixed_window_rect();
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            info->ptMinTrackSize.x = width;
            info->ptMinTrackSize.y = height;
            info->ptMaxTrackSize.x = width;
            info->ptMaxTrackSize.y = height;
            info->ptMaxSize.x = width;
            info->ptMaxSize.y = height;
        }
        return 0;
    }

    case WM_COMMAND:
        break;

    case WM_LBUTTONDOWN: {
        POINT point;
        point.x = (int)(short)LOWORD(lparam);
        point.y = (int)(short)HIWORD(lparam);
        if (PtInRect(&g_kbo_hub_league_dropdown_rect, point)) {
            kbo_hub_show_league_dropdown(hwnd);
            kbo_refresh_hotkey_window_layout(hwnd);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_team_dropdown_rect, point)) {
            kbo_hub_show_team_dropdown(hwnd);
            kbo_refresh_hotkey_window_layout(hwnd);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_lang_ko_rect, point)) {
            g_kbo_hub_language = KBO_HUB_LANG_KO;
            kbo_hub_save_language_setting();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_lang_en_rect, point)) {
            g_kbo_hub_language = KBO_HUB_LANG_EN;
            kbo_hub_save_language_setting();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_less_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, -1);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_more_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, 1);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                int direction = point.y < g_kbo_hub_scrollbar_thumb_rect.top
                    ? -kbo_hub_estimate_visible_edit_lines()
                    :  kbo_hub_estimate_visible_edit_lines();
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, direction);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        RECT wnd_rect;
        GetClientRect(hwnd, &wnd_rect);
        int wnd_w = wnd_rect.right - wnd_rect.left;
        int wnd_h = wnd_rect.bottom - wnd_rect.top;
        for (int i = 0; i < KBO_HUB_NAV_COUNT; i++) {
            RECT item = kbo_hub_nav_item_rect(i, wnd_w, wnd_h);
            if (PtInRect(&item, point)) {
                g_kbo_hub_selected_view = i;
                kbo_refresh_hotkey_window_layout(hwnd);
                return 0;
            }
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wparam;
        SetTextColor(hdc, KBO_HUB_COLOR_TEXT);
        SetBkColor(hdc, KBO_HUB_COLOR_PANEL_ALT);
        return (LRESULT)g_kbo_hub_brush_panel_alt;
    }

    case WM_TIMER:
        if (wparam == 1u
                && (GetAsyncKeyState(VK_F2) & 1) != 0
                && kbo_foreground_is_this_process()) {
            kbo_queue_hotkey_window_toggle();
            return 0;
        }
        if (wparam == 1u
                && (GetAsyncKeyState(VK_F5) & 1) != 0
                && g_kbo_hotkey_window != NULL
                && IsWindowVisible(g_kbo_hotkey_window)
                && kbo_foreground_is_this_process()) {
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (wparam == 1u && g_kbo_hotkey_window != NULL && IsWindowVisible(g_kbo_hotkey_window)) {
            static uintptr_t s_last_db_ptr = 0;
            uintptr_t cur_db = g_ootp_cached_global_db;
            if (cur_db != s_last_db_ptr) {
                s_last_db_ptr = cur_db;
                kbo_refresh_hotkey_window();
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
        }
        break;

    case KBO_WM_TOGGLE_SERVICE_MONITOR:
        kbo_show_or_hide_hotkey_window();
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1u);
        if (g_kbo_hotkey_keyboard_hook != NULL) {
            UnhookWindowsHookEx(g_kbo_hotkey_keyboard_hook);
            g_kbo_hotkey_keyboard_hook = NULL;
        }
        g_kbo_hotkey_window         = NULL;
        g_kbo_hotkey_edit           = NULL;
        g_kbo_foreign_list          = NULL;
        g_kbo_foreign_keep_button   = NULL;
        g_kbo_foreign_release_button = NULL;
        g_kbo_hotkey_edit_original_proc = NULL;
        kbo_hub_delete_gdi_objects();
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static DWORD WINAPI kbo_hotkey_window_thread(LPVOID parameter)
{
    g_kbo_hotkey_instance = (HINSTANCE)parameter;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    kbo_hub_init_gdi_objects();

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = kbo_hotkey_window_proc;
    wc.hInstance     = g_kbo_hotkey_instance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = g_kbo_hub_brush_bg;
    wc.lpszClassName = "OOTPKBOHubWindow";

    ATOM klass = RegisterClassExA(&wc);
    if (klass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        append_logf("KBO F2 hub class registration failed error=%lu", GetLastError());
        return 0;
    }

    HWND owner = kbo_find_ootp_main_window();
    DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD window_ex_style = WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE;
    RECT fixed_rect = kbo_hub_fixed_window_rect();
    HWND hwnd = CreateWindowExA(
        window_ex_style,
        wc.lpszClassName,
        "KBO Companion",
        window_style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        fixed_rect.right - fixed_rect.left,
        fixed_rect.bottom - fixed_rect.top,
        owner, NULL, g_kbo_hotkey_instance, NULL);

    if (hwnd == NULL) {
        append_logf("KBO F2 hub window creation failed error=%lu", GetLastError());
        return 0;
    }

    g_kbo_hotkey_window = hwnd;
    g_kbo_hotkey_keyboard_hook = SetWindowsHookExA(
        WH_KEYBOARD_LL, kbo_hotkey_keyboard_proc, g_kbo_hotkey_instance, 0);
    if (g_kbo_hotkey_keyboard_hook == NULL) {
        append_logf("KBO F2 hub keyboard hook failed error=%lu; falling back to polling", GetLastError());
    } else {
        append_logf("KBO F2 hub keyboard hook installed hook=%p", (void*)g_kbo_hotkey_keyboard_hook);
    }
    append_logf("KBO F2 hub ready hwnd=%p owner=%p", (void*)hwnd, (void*)owner);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    CoUninitialize();
    return 0;
}

static void start_kbo_hotkey_window_thread(HINSTANCE instance)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    if (InterlockedCompareExchange(&g_kbo_hotkey_window_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_hotkey_window_thread, instance, 0, &g_kbo_hotkey_thread_id);
    if (thread == NULL) {
        InterlockedExchange(&g_kbo_hotkey_window_started, 0);
        append_logf("KBO F2 hub thread failed error=%lu", GetLastError());
        return;
    }

    CloseHandle(thread);
}
