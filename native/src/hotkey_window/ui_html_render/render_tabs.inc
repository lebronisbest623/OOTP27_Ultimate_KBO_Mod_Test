static void kbo_webview_append_js_string(KboWindowTextBuffer* buffer, const char* text)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "'");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            unsigned char ch = (unsigned char)*p;
            if (ch == '\\' || ch == '\'') {
                kbo_window_text_appendf(buffer, "\\%c", ch);
            } else if (ch == '\n') {
                kbo_window_text_appendf(buffer, "\\n");
            } else if (ch == '\r') {
                kbo_window_text_appendf(buffer, "\\r");
            } else if (ch < 0x20u) {
                kbo_window_text_appendf(buffer, "\\x%02x", (unsigned int)ch);
            } else {
                kbo_window_text_appendf(buffer, "%c", ch);
            }
        }
    }
    kbo_window_text_appendf(buffer, "'");
}

static int kbo_hub_selected_league_is_kbo(void)
{
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    return kbo_league_id != 0u && g_kbo_hub_selected_league_id == kbo_league_id;
}

static int kbo_hub_selected_league_is_amateur_reputation_league(void)
{
    return g_kbo_hub_selected_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID
        || g_kbo_hub_selected_league_id == KBO_COLLEGE_LEAGUE_ID;
}

static int kbo_hub_view_available_for_selected_league(int view)
{
    if (view == KBO_HUB_VIEW_MOD_INFO) {
        return 1;
    }
    if (view == KBO_HUB_VIEW_REPUTATION) {
        return kbo_hub_selected_league_is_amateur_reputation_league();
    }
    return kbo_hub_selected_league_is_kbo();
}

static void kbo_webview_append_main_tabs(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    static const int main_views[] = {
        KBO_HUB_VIEW_MOD_INFO,
        KBO_HUB_VIEW_MILITARY,
        KBO_HUB_VIEW_ASIAN_QUOTA,
        KBO_HUB_VIEW_ASIAN_GAMES,
        KBO_HUB_VIEW_FA_CASES,
        KBO_HUB_VIEW_SETTINGS,
        KBO_HUB_VIEW_REPUTATION
    };
    for (size_t i = 0; i < sizeof(main_views) / sizeof(main_views[0]); i++) {
        int view = main_views[i];
        if (!kbo_hub_view_available_for_selected_league(view)) {
            continue;
        }
        kbo_window_text_appendf(buffer, "<a class='mainTab %s' href='kbo://view/%d'>",
            view == g_kbo_hub_selected_view ? "active" : "", view);
        kbo_html_append_escaped(buffer, kbo_hub_nav_label(view));
        kbo_window_text_appendf(buffer, "</a>");
    }
}

static void kbo_webview_append_sub_tabs(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO) {
        for (int i = 0; i < KBO_HUB_MOD_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://mod/%d'>",
                i == g_kbo_hub_selected_mod_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_mod_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY) {
        for (int i = 0; i < KBO_HUB_MILITARY_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://military/%d'>",
                i == g_kbo_hub_selected_military_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_military_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA) {
        for (int i = 0; i < KBO_HUB_FOREIGN_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://foreign/%d'>",
                i == g_kbo_hub_selected_foreign_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_foreign_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES) {
        for (int i = 0; i < KBO_HUB_AGAMES_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://agames/%d'>",
                i == g_kbo_hub_selected_agames_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_agames_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES) {
        for (int i = 0; i < KBO_HUB_FA_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://fa/%d'>",
                i == g_kbo_hub_selected_fa_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_fa_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    }
}

static int kbo_webview_current_view_has_sub_tabs(void)
{
    return g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS;
}
