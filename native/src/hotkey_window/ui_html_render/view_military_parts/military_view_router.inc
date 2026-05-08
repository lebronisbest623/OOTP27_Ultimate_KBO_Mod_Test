static void kbo_webview_append_military_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_RESULTS) {
        kbo_webview_append_military_results_view(buffer);
        return;
    }

    if (g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_APPLICANTS) {
        kbo_webview_append_military_applicants_view(buffer);
        return;
    }

    kbo_webview_append_military_roster_view(buffer);
}
