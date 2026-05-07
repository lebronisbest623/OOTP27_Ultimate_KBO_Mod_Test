static int read_kbo_localappdata_flag_file(const char* file_name)
{
    char key[128] = {0};
    if (!kbo_flag_key_from_file_name(file_name, key, sizeof(key))) {
        return 0;
    }

    int value = 0;
    return kbo_read_localappdata_json_flag_value(key, file_name, &value) ? value : 0;
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
