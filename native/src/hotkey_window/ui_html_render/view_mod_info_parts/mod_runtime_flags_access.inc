static const KboModRuntimeFlagSetting* kbo_find_mod_runtime_flag_setting(const char* key)
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

static int kbo_get_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting)
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

static int kbo_set_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting, int enabled)
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
