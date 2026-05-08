static void kbo_hub_clear_league_display_cache(void)
{
    memset(g_kbo_league_display_cache, 0, sizeof(g_kbo_league_display_cache));
    memset(g_kbo_league_ptr_miss_cache_ids, 0, sizeof(g_kbo_league_ptr_miss_cache_ids));
    memset(g_kbo_league_ptr_miss_cache_until_ms, 0, sizeof(g_kbo_league_ptr_miss_cache_until_ms));
    g_kbo_league_ptr_cache_id = 0;
    g_kbo_league_ptr_cache_ptr = 0;
    g_kbo_league_display_cache_prewarmed_global = 0;
}

static void kbo_hub_refresh_league_cache_context(void)
{
    uintptr_t global = get_ootp_global_database();
    if (global != g_kbo_league_display_cache_global) {
        g_kbo_league_display_cache_global = global;
        kbo_hub_clear_league_display_cache();
    }
}

static int kbo_hub_find_league_display_cache_slot(uint32_t league_id)
{
    for (int i = 0; i < KBO_LEAGUE_DISPLAY_CACHE_MAX; i++) {
        if (g_kbo_league_display_cache[i].league_id == league_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_hub_find_league_display_cache_insert_slot(uint32_t league_id)
{
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot >= 0) {
        return slot;
    }

    for (int i = 0; i < KBO_LEAGUE_DISPLAY_CACHE_MAX; i++) {
        if (g_kbo_league_display_cache[i].league_id == 0u) {
            return i;
        }
    }

    return (int)(league_id % KBO_LEAGUE_DISPLAY_CACHE_MAX);
}

static void kbo_hub_store_league_display_cache(
    uint32_t league_id,
    uintptr_t league_ptr,
    int score,
    const char* name,
    const char* logo_file)
{
    if (league_id == 0 || league_ptr == 0 || name == NULL || name[0] == '\0') {
        return;
    }

    int slot = kbo_hub_find_league_display_cache_insert_slot(league_id);
    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->league_id == league_id
            && entry->name[0] != '\0'
            && entry->score > score) {
        return;
    }

    entry->league_id = league_id;
    entry->league_ptr = league_ptr;
    entry->score = score;
    entry->year = 0;
    entry->logo_file[0] = '\0';
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        entry->year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    }
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    if (logo_file != NULL && logo_file[0] != '\0') {
        snprintf(entry->logo_file, sizeof(entry->logo_file), "%s", logo_file);
    }
}

static int kbo_hub_try_copy_cached_league_name(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->name[0] == '\0' || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        return 0;
    }

    snprintf(out, out_size, "%s", entry->name);
    return 1;
}

static int kbo_hub_try_copy_cached_league_logo_file(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->logo_file[0] == '\0' || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        return 0;
    }

    snprintf(out, out_size, "%s", entry->logo_file);
    return 1;
}

