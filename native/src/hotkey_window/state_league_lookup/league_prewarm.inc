static void kbo_hub_prewarm_league_display_cache(void)
{
    kbo_hub_refresh_league_cache_context();
    uintptr_t global = g_kbo_league_display_cache_global;
    if (global == 0 || g_kbo_league_display_cache_prewarmed_global == global) {
        return;
    }

    uint32_t league_ids[KBO_LEAGUE_DISPLAY_CACHE_MAX] = {0};
    int league_count = kbo_hub_collect_visible_league_ids(league_ids, KBO_LEAGUE_DISPLAY_CACHE_MAX);
    if (league_count <= 0) {
        return;
    }

    ULONGLONG started_ms = GetTickCount64();
    int found = kbo_scan_named_league_ptrs_for_ids(league_ids, league_count, (SIZE_T)0x00040000u);
    if (found < league_count) {
        found = kbo_scan_named_league_ptrs_for_ids(league_ids, league_count, (SIZE_T)0x00400000u);
    }
    g_kbo_league_display_cache_prewarmed_global = global;
    append_logf(
        "KBO: F2 league cache prewarmed leagues=%d found=%d ms=%llu",
        league_count,
        found,
        (unsigned long long)(GetTickCount64() - started_ms));
}

