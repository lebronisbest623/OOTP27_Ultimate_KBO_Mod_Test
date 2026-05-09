#include "../../../hotkey_window_runtime_internal.h"

int kbo_webview_handle_view_navigation_command(const char* cmd)
{

    if (strncmp(cmd, "view/", 5) == 0) {
        int view = atoi(cmd + 5);
        if (view >= 0 && view < KBO_HUB_NAV_COUNT) {
            if (!kbo_hub_view_available_for_selected_league(view)) {
                view = KBO_HUB_VIEW_MOD_INFO;
            }
            if (view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
                g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_QUOTA;
                g_kbo_hub_selected_foreign_subview = KBO_HUB_FOREIGN_SUBVIEW_RIGHTS;
            } else if (view == KBO_HUB_VIEW_UPCOMING_FA) {
                g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
                g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
            } else {
                g_kbo_hub_selected_view = view;
            }
            g_kbo_hub_open_dropdown = 0;
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
                    && (g_kbo_hub_selected_foreign_subview < 0
                        || g_kbo_hub_selected_foreign_subview >= KBO_HUB_FOREIGN_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_foreign_subview = KBO_HUB_FOREIGN_SUBVIEW_ROSTER;
            }
            if (view == KBO_HUB_VIEW_ASIAN_GAMES
                    && (g_kbo_hub_selected_agames_subview < 0
                        || g_kbo_hub_selected_agames_subview >= KBO_HUB_AGAMES_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_agames_subview = KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS;
            }
            if (view == KBO_HUB_VIEW_MILITARY
                    && (g_kbo_hub_selected_military_subview < 0
                        || g_kbo_hub_selected_military_subview >= KBO_HUB_MILITARY_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_military_subview = KBO_HUB_MILITARY_SUBVIEW_ROSTER;
            }
            if (view == KBO_HUB_VIEW_MOD_INFO
                    && (g_kbo_hub_selected_mod_subview < 0
                        || g_kbo_hub_selected_mod_subview >= KBO_HUB_MOD_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_README;
            }
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES
                    && (g_kbo_hub_selected_fa_subview < 0
                        || g_kbo_hub_selected_fa_subview >= KBO_HUB_FA_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
            }
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "military/results/year/", 22) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        uint32_t year = (uint32_t)strtoul(cmd + 22, NULL, 10);
        if (year >= 1982u && year <= 2300u) {
            g_kbo_hub_selected_military_results_year = year;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MILITARY;
        g_kbo_hub_selected_military_subview = KBO_HUB_MILITARY_SUBVIEW_RESULTS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "military/", 9) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 9);
        if (subview >= 0 && subview < KBO_HUB_MILITARY_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MILITARY;
            g_kbo_hub_selected_military_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "foreign/", 8) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 8);
        if (subview >= 0 && subview < KBO_HUB_FOREIGN_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_QUOTA;
            g_kbo_hub_selected_foreign_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    return 0;
}
