#include "flags_api.h"

#include "flag_key.h"
#include "localappdata_reader.h"
#include "../core_log.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static volatile LONG g_kbo_runtime_threads_stop_requested = 0;

void kbo_request_runtime_threads_stop(void)
{
    InterlockedExchange(&g_kbo_runtime_threads_stop_requested, 1);
}

int kbo_runtime_threads_should_continue(void)
{
    if (InterlockedCompareExchange(&g_kbo_runtime_threads_stop_requested, 0, 0) != 0) {
        return 0;
    }

    int configured = 1;
    if (kbo_read_localappdata_json_flag_value("enable_kbo_fix", "enable_kbo_fix.txt", &configured)
            && !configured) {
        return 0;
    }
    return 1;
}

int kbo_runtime_sleep_should_continue(uint32_t total_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < total_ms) {
        if (!kbo_runtime_threads_should_continue()) {
            return 0;
        }

        uint32_t step = total_ms - elapsed;
        if (step > 250u) {
            step = 250u;
        }
        Sleep(step);
        elapsed += step;
    }
    return kbo_runtime_threads_should_continue();
}

int read_kbo_localappdata_flag_file(const char* file_name)
{
    char key[128] = {0};
    if (!kbo_flag_key_from_file_name(file_name, key, sizeof(key))) {
        return 0;
    }

    typedef struct KboFlagCacheEntry {
        char key[128];
        int value;
        DWORD tick;
        int valid;
    } KboFlagCacheEntry;
    static KboFlagCacheEntry cache[128];
    static volatile LONG cache_lock = 0;

    DWORD now = GetTickCount();
    while (InterlockedCompareExchange(&cache_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < (int)(sizeof(cache) / sizeof(cache[0])); i++) {
        if (cache[i].valid && strcmp(cache[i].key, key) == 0 && now - cache[i].tick < 2000u) {
            int cached = cache[i].value;
            InterlockedExchange(&cache_lock, 0);
            return cached;
        }
    }
    InterlockedExchange(&cache_lock, 0);

    int value = 0;
    int found = kbo_read_localappdata_json_flag_value(key, file_name, &value);
    value = found ? (value ? 1 : 0) : 0;

    while (InterlockedCompareExchange(&cache_lock, 1, 0) != 0) {
        Sleep(0);
    }
    int slot = -1;
    for (int i = 0; i < (int)(sizeof(cache) / sizeof(cache[0])); i++) {
        if (cache[i].valid && strcmp(cache[i].key, key) == 0) {
            slot = i;
            break;
        }
        if (slot < 0 && !cache[i].valid) {
            slot = i;
        }
    }
    if (slot < 0) {
        slot = (int)(now % (DWORD)(sizeof(cache) / sizeof(cache[0])));
    }
    snprintf(cache[slot].key, sizeof(cache[slot].key), "%s", key);
    cache[slot].value = value;
    cache[slot].tick = now;
    cache[slot].valid = 1;
    InterlockedExchange(&cache_lock, 0);
    return value;
}

#define KBO_INTL_ESTABLISHED_FA_MULTIPLIER_KEY "intl_established_fa_multiplier"
#define KBO_INTL_ESTABLISHED_FA_MULTIPLIER_DEFAULT 20
#define KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MIN 1
#define KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MAX 20

int kbo_clamp_intl_established_fa_multiplier(int value)
{
    if (value < KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MIN) {
        return KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MIN;
    }
    if (value > KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MAX) {
        return KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MAX;
    }
    return value;
}

int kbo_get_intl_established_fa_multiplier(void)
{
    int value = KBO_INTL_ESTABLISHED_FA_MULTIPLIER_DEFAULT;
    if (!kbo_read_localappdata_json_int_value(KBO_INTL_ESTABLISHED_FA_MULTIPLIER_KEY, &value)) {
        value = KBO_INTL_ESTABLISHED_FA_MULTIPLIER_DEFAULT;
    }
    return kbo_clamp_intl_established_fa_multiplier(value);
}

int kbo_set_intl_established_fa_multiplier(int value)
{
    return kbo_write_localappdata_json_int_value(
        KBO_INTL_ESTABLISHED_FA_MULTIPLIER_KEY,
        kbo_clamp_intl_established_fa_multiplier(value));
}

static const char* KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[9] = {
    "foreign_fa_demand_minimum_salary",
    "foreign_fa_demand_poor_salary",
    "foreign_fa_demand_fair_salary",
    "foreign_fa_demand_below_average_salary",
    "foreign_fa_demand_average_salary",
    "foreign_fa_demand_above_average_salary",
    "foreign_fa_demand_good_salary",
    "foreign_fa_demand_star_salary",
    "foreign_fa_demand_superstar_salary"
};

static const char* KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[9] = {
    "asian_quota_fa_demand_minimum_salary",
    "asian_quota_fa_demand_poor_salary",
    "asian_quota_fa_demand_fair_salary",
    "asian_quota_fa_demand_below_average_salary",
    "asian_quota_fa_demand_average_salary",
    "asian_quota_fa_demand_above_average_salary",
    "asian_quota_fa_demand_good_salary",
    "asian_quota_fa_demand_star_salary",
    "asian_quota_fa_demand_superstar_salary"
};

static const int32_t KBO_FOREIGN_FA_DEMAND_BASELINE_DEFAULTS[9] = {
    300000, 300000, 320000, 350000, 450000, 600000, 800000, 1000000, 1500000
};

static const int32_t KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_DEFAULTS[9] = {
    80000, 100000, 120000, 150000, 200000, 300000, 450000, 650000, 900000
};

int32_t kbo_clamp_foreign_fa_demand_baseline_value(int32_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 20000000) {
        return 20000000;
    }
    return value;
}

int32_t kbo_get_foreign_fa_demand_baseline_value(int index)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    int value = KBO_FOREIGN_FA_DEMAND_BASELINE_DEFAULTS[index];
    if (!kbo_read_localappdata_json_int_value(KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[index], &value)) {
        value = KBO_FOREIGN_FA_DEMAND_BASELINE_DEFAULTS[index];
    }
    return kbo_clamp_foreign_fa_demand_baseline_value(value);
}

int32_t kbo_get_asian_quota_fa_demand_baseline_value(int index)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    int value = KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_DEFAULTS[index];
    if (!kbo_read_localappdata_json_int_value(KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[index], &value)) {
        value = KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_DEFAULTS[index];
    }
    return kbo_clamp_foreign_fa_demand_baseline_value(value);
}

int32_t kbo_get_foreign_fa_demand_baseline_value_for_player(int index, int asian_quota)
{
    return asian_quota
        ? kbo_get_asian_quota_fa_demand_baseline_value(index)
        : kbo_get_foreign_fa_demand_baseline_value(index);
}

int kbo_set_foreign_fa_demand_baseline_value(int index, int32_t value)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    return kbo_write_localappdata_json_int_value(
        KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[index],
        kbo_clamp_foreign_fa_demand_baseline_value(value));
}

int kbo_set_asian_quota_fa_demand_baseline_value(int index, int32_t value)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    return kbo_write_localappdata_json_int_value(
        KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[index],
        kbo_clamp_foreign_fa_demand_baseline_value(value));
}

#define KBO_FOREIGN_FA_QUALITY_CAP_ENABLED_KEY "foreign_fa_quality_cap_enabled"

int kbo_get_foreign_fa_quality_cap_enabled_setting(void)
{
    int value = 1;
    if (!kbo_read_localappdata_json_flag_value(
            KBO_FOREIGN_FA_QUALITY_CAP_ENABLED_KEY,
            "foreign_fa_quality_cap_enabled.txt",
            &value)) {
        if (read_kbo_localappdata_flag_file("disable_intl_established_fa_quality_shaping.txt")
                || read_kbo_localappdata_flag_file("disable_intl_established_fa_generation_filter.txt")) {
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
