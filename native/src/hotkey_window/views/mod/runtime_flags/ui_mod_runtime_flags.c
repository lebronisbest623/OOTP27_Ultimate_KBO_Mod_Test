#include "../info/ui_mod_info_views_internal.h"

struct KboModRuntimeFlagSetting {
    const char* key;
    const char* label;
    int enabled_value;
    int default_enabled;
    const char* companion_enable_key;
    int category;
};

static const KboModRuntimeFlagSetting KBO_MOD_RUNTIME_FLAG_SETTINGS[] = {
    { "enable_foreign_waiver_ai", "Foreign waiver AI", 1, 1, NULL, KBO_MOD_FLAG_USER },
    { "enable_single_division_allstar_events", "Single-division All-Star events", 1, 1, NULL, KBO_MOD_FLAG_USER },

    { "enable_experimental_runtime_hooks", "Runtime patch engine", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_foreign_waiver_background_scanner", "Foreign waiver background scanner", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_foreign_waiver_legacy_auto_detector", "Foreign waiver legacy detector", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_foreign_trade_check_patch", "Foreign trade quota guard", 0, 1, "enable_kbo_foreign_trade_check_patch", KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_custom_foreign_policy", "Custom foreign policy", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_foreign_injury_replacement", "Foreign injury replacement", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_intl_established_fa_generation_filter", "International FA generation filter", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_fa_compensation", "FA compensation", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_salary_arbitration_no_withdraw_patch", "Salary arbitration no-withdraw", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_no_minor_contract_patch", "No minor-contract patch", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_sangmu_fa_block_core", "Military team FA block", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_military_team_add_guard_patch", "Military team add guard", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_amateur_assignment_reroute", "Amateur reputation assignment", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_single_division_allstar_runtime_patches", "Single-division All-Star runtime", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_single_division_allstar_voting_hook", "Single-division All-Star voting", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_single_division_allstar_settings_patch", "Single-division All-Star settings", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_fa_salary_opening_day_snapshot", "FA salary opening-day snapshot", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_ai_fa_status_candidate_insert_hook", "AI FA candidate hook", 1, 0, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_ai_fa_fallback_patch", "AI FA fallback patch", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_player_team_signability_patch", "Player-team signability patch", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_offer_eligibility_patch", "Offer eligibility patch", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_submit_offer_probe_patch", "Submit-offer probe", 0, 1, "enable_kbo_submit_offer_probe_patch", KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_foreign_signing_branch_patch", "Foreign signing branch hook", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "disable_kbo_runtime_roster_marker_guard", "Runtime roster marker guard", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_callup_foreign_limit_patch", "Foreign call-up limit patch", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_intl_established_fa_quality_probe_patch", "International FA quality probe", 1, 1, NULL, KBO_MOD_FLAG_RECOVERY },
    { "enable_kbo_season_phase_monitor", "Season phase monitor", 0, 1, NULL, KBO_MOD_FLAG_RECOVERY },

    { "enable_amateur_assignment_verbose_log", "Amateur assignment verbose log", 1, 0, NULL, KBO_MOD_FLAG_DIAGNOSTIC },

    { "enable_fa_requalification", "FA requalification", 1, 0, NULL, KBO_MOD_FLAG_LEGACY },
    { "enable_kbo_fa_signability_hooks", "Legacy FA signability hooks", 1, 0, NULL, KBO_MOD_FLAG_LEGACY }
};

const KboModRuntimeFlagSetting* kbo_find_mod_runtime_flag_setting(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS) / sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS[0]); i++) {
        if (strcmp(KBO_MOD_RUNTIME_FLAG_SETTINGS[i].key, key) == 0) {
            return &KBO_MOD_RUNTIME_FLAG_SETTINGS[i];
        }
    }
    return NULL;
}

int kbo_get_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting)
{
    if (setting == NULL || setting->key == NULL || setting->key[0] == '\0') {
        return 0;
    }
    int raw_value = setting->enabled_value ? 0 : 1;
    if (!kbo_read_localappdata_json_flag_value(setting->key, setting->key, &raw_value)) {
        return setting->default_enabled ? 1 : 0;
    }
    return raw_value == setting->enabled_value ? 1 : 0;
}

int kbo_set_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting, int enabled)
{
    if (setting == NULL || setting->key == NULL || setting->key[0] == '\0') {
        return 0;
    }
    int raw_value = enabled ? setting->enabled_value : (setting->enabled_value ? 0 : 1);
    int ok = kbo_write_localappdata_json_int_value(setting->key, raw_value);
    if (ok && enabled && setting->companion_enable_key != NULL && setting->companion_enable_key[0] != '\0') {
        ok = kbo_write_localappdata_json_int_value(setting->companion_enable_key, 1);
    }
    return ok;
}

void kbo_webview_append_mod_runtime_flag_row(KboWindowTextBuffer* buffer, const KboModRuntimeFlagSetting* setting)
{
    if (buffer == NULL || setting == NULL || setting->key == NULL || setting->label == NULL) {
        return;
    }
    int enabled = kbo_get_mod_runtime_flag_enabled(setting);
    kbo_window_text_appendf(
        buffer,
        "<div class='settingRow flagSettingRow'><label class='settingLabel' for='flag_%s'>",
        setting->key);
    kbo_html_append_escaped(buffer, setting->label);
    char id[128] = {0};
    char href_on[192] = {0};
    char href_off[192] = {0};
    snprintf(id, sizeof(id), "flag_%s", setting->key);
    snprintf(href_on, sizeof(href_on), "kbo://mod/settings/flag/%s/on", setting->key);
    snprintf(href_off, sizeof(href_off), "kbo://mod/settings/flag/%s/off", setting->key);
    kbo_window_text_appendf(buffer, "</label>");
    kbo_webview_begin_ootp_choice(buffer, id, enabled ? "On" : "Off");
    kbo_webview_append_ootp_choice_option(buffer, href_on, "On", enabled);
    kbo_webview_append_ootp_choice_option(buffer, href_off, "Off", !enabled);
    kbo_webview_end_ootp_choice(buffer);
    kbo_window_text_appendf(buffer, "</div>");
}

void kbo_webview_append_mod_runtime_flag_group(
    KboWindowTextBuffer* buffer,
    int category,
    const char* title,
    const char* help_text)
{
    if (buffer == NULL || title == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<div class='flagGroup'><h3>");
    kbo_html_append_escaped(buffer, title);
    kbo_window_text_appendf(buffer, "</h3>");
    if (help_text != NULL && help_text[0] != '\0') {
        kbo_window_text_appendf(buffer, "<p>");
        kbo_html_append_escaped(buffer, help_text);
        kbo_window_text_appendf(buffer, "</p>");
    }
    for (size_t i = 0; i < sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS) / sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS[0]); i++) {
        if (KBO_MOD_RUNTIME_FLAG_SETTINGS[i].category == category) {
            kbo_webview_append_mod_runtime_flag_row(buffer, &KBO_MOD_RUNTIME_FLAG_SETTINGS[i]);
        }
    }
    kbo_window_text_appendf(buffer, "</div>");
}

