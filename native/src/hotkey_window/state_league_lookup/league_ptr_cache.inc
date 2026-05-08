static int kbo_hub_try_get_cached_league_ptr(uint32_t league_id, uintptr_t* out_league_ptr)
{
    if (out_league_ptr != NULL) {
        *out_league_ptr = 0;
    }

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->league_ptr == 0
            || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE
            || !memory_range_readable((void*)entry->league_ptr, KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN)) {
        memset(entry, 0, sizeof(*entry));
        return 0;
    }

    if (out_league_ptr != NULL) {
        *out_league_ptr = entry->league_ptr;
    }
    return 1;
}

static int kbo_league_ptr_recent_miss(uint32_t league_id, ULONGLONG now_ms)
{
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id
                && now_ms < g_kbo_league_ptr_miss_cache_until_ms[i]) {
            return 1;
        }
    }
    return 0;
}

static void kbo_remember_league_ptr_miss(uint32_t league_id, ULONGLONG now_ms)
{
    int slot = -1;
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id) {
            slot = i;
            break;
        }
        if (slot < 0 && (g_kbo_league_ptr_miss_cache_ids[i] == 0u
                || now_ms >= g_kbo_league_ptr_miss_cache_until_ms[i])) {
            slot = i;
        }
    }
    if (slot < 0) {
        slot = (int)(league_id % KBO_LEAGUE_PTR_MISS_CACHE_MAX);
    }
    g_kbo_league_ptr_miss_cache_ids[slot] = league_id;
    g_kbo_league_ptr_miss_cache_until_ms[slot] = now_ms + 3000u;
}

static void kbo_forget_league_ptr_miss(uint32_t league_id)
{
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id) {
            g_kbo_league_ptr_miss_cache_ids[i] = 0;
            g_kbo_league_ptr_miss_cache_until_ms[i] = 0;
        }
    }
}

