#include "../flags_api.h"

#include "../../localappdata/localappdata_reader.h"
#include "../../../logging/core_log.h"

#include <windows.h>

#define KBO_FOREIGN_FA_QUALITY_CAP_ENABLED_KEY "foreign_fa_quality_cap_enabled"

int kbo_get_foreign_fa_quality_cap_enabled_setting(void)
{
    int value = 1;
    if (!kbo_read_localappdata_json_flag_value(
            KBO_FOREIGN_FA_QUALITY_CAP_ENABLED_KEY,
            "foreign_fa_quality_cap_enabled.txt",
            &value)) {
        if (read_kbo_localappdata_flag_file("disable_intl_established_fa_quality_shaping.txt")) {
            value = 0;
        } else {
            value = 1;
        }
    }
    return value ? 1 : 0;
}

int kbo_set_foreign_fa_quality_cap_enabled_setting(int enabled)
{
    return kbo_write_localappdata_json_int_value(
        KBO_FOREIGN_FA_QUALITY_CAP_ENABLED_KEY,
        enabled ? 1 : 0);
}

int kbo_get_profiler_enabled_setting(void)
{
    int value = 0;
    if (!kbo_read_localappdata_json_flag_value("enable_kbo_profiler", "enable_kbo_profiler.txt", &value)) {
        value = 0;
    }
    return value ? 1 : 0;
}

int kbo_set_profiler_enabled_setting(int enabled)
{
    return kbo_write_localappdata_json_int_value("enable_kbo_profiler", enabled ? 1 : 0);
}

#define KBO_ALLOW_ALL_UI_TEAM_ACTIONS_KEY "allow_all_ui_team_actions"

int kbo_get_allow_all_ui_team_actions_setting(void)
{
    int value = 1;
    if (!kbo_read_localappdata_json_flag_value(
            KBO_ALLOW_ALL_UI_TEAM_ACTIONS_KEY,
            "allow_all_ui_team_actions.txt",
            &value)) {
        value = 1;
    }
    return value ? 1 : 0;
}

int kbo_set_allow_all_ui_team_actions_setting(int enabled)
{
    return kbo_write_localappdata_json_int_value(KBO_ALLOW_ALL_UI_TEAM_ACTIONS_KEY, enabled ? 1 : 0);
}

int kbo_fix_enabled(void)
{
    /* The DLL is purpose-injected by KBOLauncher, so it is enabled by default.
       Set enable_kbo_fix=false in kbo_flags.json to opt out. */
    static LONG cached = -1;
    LONG value = cached;
    if (value != -1) {
        return value == 1;
    }

    int enabled = 1;
    int configured = 0;
    if (kbo_read_localappdata_json_flag_value("enable_kbo_fix", "enable_kbo_fix.txt", &configured)) {
        enabled = configured ? 1 : 0;
    }

    InterlockedCompareExchange(&cached, enabled ? 1 : 0, -1);
    append_logf("KBO fix opt-in=%d", enabled ? 1 : 0);
    return enabled;
}
