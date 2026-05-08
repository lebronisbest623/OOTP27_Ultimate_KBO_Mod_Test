static int read_kbo_localappdata_flag_file(const char* file_name)
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

static int kbo_clamp_intl_established_fa_multiplier(int value)
{
    if (value < KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MIN) {
        return KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MIN;
    }
    if (value > KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MAX) {
        return KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MAX;
    }
    return value;
}

static int kbo_get_intl_established_fa_multiplier(void)
{
    int value = KBO_INTL_ESTABLISHED_FA_MULTIPLIER_DEFAULT;
    if (!kbo_read_localappdata_json_int_value(KBO_INTL_ESTABLISHED_FA_MULTIPLIER_KEY, &value)) {
        value = KBO_INTL_ESTABLISHED_FA_MULTIPLIER_DEFAULT;
    }
    return kbo_clamp_intl_established_fa_multiplier(value);
}

static int kbo_set_intl_established_fa_multiplier(int value)
{
    return kbo_write_localappdata_json_int_value(
        KBO_INTL_ESTABLISHED_FA_MULTIPLIER_KEY,
        kbo_clamp_intl_established_fa_multiplier(value));
}

static int kbo_get_profiler_enabled_setting(void)
{
    int value = 0;
    if (!kbo_read_localappdata_json_flag_value("enable_kbo_profiler", "enable_kbo_profiler.txt", &value)) {
        value = 0;
    }
    return value ? 1 : 0;
}

static int kbo_set_profiler_enabled_setting(int enabled)
{
    return kbo_write_localappdata_json_int_value("enable_kbo_profiler", enabled ? 1 : 0);
}

#define KBO_ALLOW_ALL_UI_TEAM_ACTIONS_KEY "allow_all_ui_team_actions"

static int kbo_get_allow_all_ui_team_actions_setting(void)
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

static int kbo_set_allow_all_ui_team_actions_setting(int enabled)
{
    return kbo_write_localappdata_json_int_value(KBO_ALLOW_ALL_UI_TEAM_ACTIONS_KEY, enabled ? 1 : 0);
}

static int kbo_fix_enabled(void)
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
