static int kbo_fa_filing_negative_cache_contains(uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < KBO_FA_FILING_NEGATIVE_CACHE_MAX; i++) {
        if (g_kbo_fa_filing_negative_cache[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_fa_filing_negative_cache_add(uint32_t player_id)
{
    if (player_id == 0u) {
        return;
    }
    LONG slot = InterlockedIncrement(&g_kbo_fa_filing_negative_cache_cursor);
    g_kbo_fa_filing_negative_cache[(uint32_t)slot % KBO_FA_FILING_NEGATIVE_CACHE_MAX] = player_id;
}

static void kbo_fa_filing_negative_cache_clear(void)
{
    memset(g_kbo_fa_filing_negative_cache, 0, sizeof(g_kbo_fa_filing_negative_cache));
    InterlockedExchange(&g_kbo_fa_filing_negative_cache_cursor, 0);
}
