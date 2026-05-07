static void kbo_refresh_hotkey_window(void)
{
    if (g_kbo_hotkey_edit == NULL) {
        return;
    }

    char* text = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 65536u);
    if (text == NULL) {
        SetWindowTextA(g_kbo_hotkey_edit, "Ultimate KBO\r\n\r\nCould not prepare this page.");
        return;
    }

    kbo_build_hub_window_text(text, 65536u);
    HFONT content_font = (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES)
        ? g_kbo_hub_font_mono : g_kbo_hub_font_body;
    if (content_font != NULL) {
        SendMessageA(g_kbo_hotkey_edit, WM_SETFONT, (WPARAM)content_font, TRUE);
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len > 0) {
        WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len) > 0) {
                SetWindowTextW(g_kbo_hotkey_edit, wide);
            } else {
                SetWindowTextA(g_kbo_hotkey_edit, text);
            }
            HeapFree(GetProcessHeap(), 0, wide);
        } else {
            SetWindowTextA(g_kbo_hotkey_edit, text);
        }
    } else {
        SetWindowTextA(g_kbo_hotkey_edit, text);
    }
    HeapFree(GetProcessHeap(), 0, text);
}
