/* Foreign waiver policy flags and league resolution. Included from native/src/foreign_waiver_ai.inc. */

static int kbo_foreign_waiver_ai_enabled(void)
{
    static LONG cached = -1;
    LONG value = cached;
    if (value != -1) {
        return value == 1;
    }
    value = read_kbo_localappdata_flag_file("enable_foreign_waiver_ai.txt") ? 1 : 0;
    InterlockedCompareExchange(&cached, value, -1);
    return cached == 1;
}

static int kbo_custom_foreign_policy_enabled(void)
{
    static LONG cached_disabled = -1;
    LONG disabled = cached_disabled;
    if (disabled == -1) {
        disabled = read_kbo_localappdata_flag_file("disable_kbo_custom_foreign_policy.txt") ? 1 : 0;
        InterlockedCompareExchange(&cached_disabled, disabled, -1);
        disabled = cached_disabled;
    }
    return kbo_fix_enabled() && disabled != 1;
}

uint32_t kbo_get_foreign_waiver_league_id(void)
{
    static uint32_t cached_league_id = 0u;
    if (cached_league_id == 0u) {
        cached_league_id = read_u32_leading_number_from_file("foreign_waiver_league_id.txt");
        if (cached_league_id == 0u) {
            cached_league_id = kbo_resolve_kbo_league_id();
        }
        if (cached_league_id != 0u) {
            append_logf("foreign waiver: resolved league id=%u", cached_league_id);
        }
    }
    return cached_league_id;
}

static int kbo_foreign_waiver_legacy_auto_detector_enabled(void)
{
    static LONG cached = -1;
    LONG value = cached;
    if (value != -1) {
        return value == 1;
    }
    value = read_kbo_localappdata_flag_file("disable_foreign_waiver_legacy_auto_detector.txt") ? 0 : 1;
    InterlockedCompareExchange(&cached, value, -1);
    return cached == 1;
}
