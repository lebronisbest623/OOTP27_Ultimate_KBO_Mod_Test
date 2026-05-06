static void kbo_webview_append_selected_view(KboWindowTextBuffer* buffer, uint32_t current_year, const char* window_status)
{
    (void)current_year;
    if (buffer == NULL) {
        return;
    }

    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY) {
        kbo_webview_append_military_view(buffer);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
        kbo_webview_append_foreign_rights_view(buffer, window_status);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA) {
        kbo_webview_append_asian_quota_view(buffer);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES) {
        kbo_webview_append_asian_games_view(buffer);
    } else {
        kbo_webview_append_fallback_text_view(buffer);
    }
}
