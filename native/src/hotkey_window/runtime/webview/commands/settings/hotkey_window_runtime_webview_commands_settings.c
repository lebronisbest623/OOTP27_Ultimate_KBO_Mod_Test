#include "../../../hotkey_window_runtime_internal.h"

int kbo_webview_handle_settings_command(const char* cmd)
{

    if (strncmp(cmd, "settings/intl-fa-multiplier/", 29) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int multiplier = kbo_clamp_intl_established_fa_multiplier(atoi(cmd + 29));
        if (kbo_set_intl_established_fa_multiplier(multiplier)) {
            append_logf("settings webview: international established FA multiplier=%d", multiplier);
        } else {
            append_logf("settings webview: failed to write international established FA multiplier=%d", multiplier);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings/foreign-fa-quality-cap/", 32) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        const char* value = cmd + 32;
        int enabled = ascii_equals_ignore_case(value, "on")
            || ascii_equals_ignore_case(value, "1")
            || ascii_equals_ignore_case(value, "true")
            || ascii_equals_ignore_case(value, "enabled");
        if (kbo_set_foreign_fa_quality_cap_enabled_setting(enabled)) {
            append_logf("settings webview: foreign FA quality cap enabled=%d", enabled ? 1 : 0);
        } else {
            append_logf("settings webview: failed to write foreign FA quality cap enabled=%d", enabled ? 1 : 0);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings/asian-quota-salary-limit/", 35) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int value = atoi(cmd + 35);
        int clamped = kbo_clamp_asian_quota_salary_limit_value(value);
        if (kbo_set_asian_quota_salary_limit(value)) {
            append_logf("settings webview: Asian quota salary limit=%d", clamped);
        } else {
            append_logf("settings webview: failed to write Asian quota salary limit=%d", value);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    const char* non_asian_quality_cap_prefix = "settings/foreign-fa-non-asian-quality-cap/";
    size_t non_asian_quality_cap_prefix_len = strlen(non_asian_quality_cap_prefix);
    if (strncmp(cmd, non_asian_quality_cap_prefix, non_asian_quality_cap_prefix_len) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        const char* rest = cmd + non_asian_quality_cap_prefix_len;
        int index = atoi(rest);
        const char* slash = strchr(rest, '/');
        int value = slash != NULL ? atoi(slash + 1) : 0;
        int clamped = kbo_clamp_foreign_fa_quality_cap_value(value);
        if (kbo_set_foreign_fa_non_asian_quality_cap_value(index, value)) {
            append_logf("settings webview: non-Asian foreign FA quality cap index=%d value=%d", index, clamped);
        } else {
            append_logf("settings webview: failed to write non-Asian foreign FA quality cap index=%d value=%d", index, value);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings/foreign-fa-baseline/", 29) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        const char* rest = cmd + 29;
        int index = atoi(rest);
        const char* slash = strchr(rest, '/');
        int value = slash != NULL ? atoi(slash + 1) : 0;
        if (kbo_set_foreign_fa_demand_baseline_value(index, value)) {
            append_logf("settings webview: foreign FA demand baseline index=%d value=%d", index, kbo_clamp_foreign_fa_demand_baseline_value(value));
        } else {
            append_logf("settings webview: failed to write foreign FA demand baseline index=%d value=%d", index, value);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings/asian-quota-fa-baseline/", 34) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        const char* rest = cmd + 34;
        int index = atoi(rest);
        const char* slash = strchr(rest, '/');
        int value = slash != NULL ? atoi(slash + 1) : 0;
        if (kbo_set_asian_quota_fa_demand_baseline_value(index, value)) {
            append_logf("settings webview: Asian quota FA demand baseline index=%d value=%d", index, kbo_clamp_foreign_fa_demand_baseline_value(value));
        } else {
            append_logf("settings webview: failed to write Asian quota FA demand baseline index=%d value=%d", index, value);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    return 0;
}
