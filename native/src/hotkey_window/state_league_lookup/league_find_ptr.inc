static uintptr_t kbo_find_league_ptr(uint32_t league_id)
{
    if (league_id == 0) {
        return 0;
    }

    kbo_hub_refresh_league_cache_context();

    ULONGLONG now_ms = GetTickCount64();
    if (kbo_league_ptr_recent_miss(league_id, now_ms)) {
        return 0;
    }

    uintptr_t cached_league_ptr = 0;
    if (kbo_hub_try_get_cached_league_ptr(league_id, &cached_league_ptr)) {
        return cached_league_ptr;
    }

    if (g_kbo_league_ptr_cache_id == league_id && g_kbo_league_ptr_cache_ptr != 0) {
        int cached_score = kbo_hub_named_league_candidate_score(
            g_kbo_league_ptr_cache_ptr,
            league_id,
            NULL,
            0,
            NULL,
            0);
        if (cached_score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
            return g_kbo_league_ptr_cache_ptr;
        }
        g_kbo_league_ptr_cache_id  = 0;
        g_kbo_league_ptr_cache_ptr = 0;
    }

    append_logf("KBO: named league ptr scan started id=%u", league_id);

    int score = -1000;
    char name[96] = {0};
    uintptr_t league_ptr = kbo_scan_named_league_ptr(league_id, (SIZE_T)0x00040000u, &score, name, sizeof(name));
    if (score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        league_ptr = kbo_scan_named_league_ptr(league_id, (SIZE_T)0x00400000u, &score, name, sizeof(name));
    }

    if (league_ptr != 0 && score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        uint32_t year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        append_logf("KBO: named league ptr FOUND id=%u ptr=%p score=%d year=%u name=%s",
            league_id, (void*)league_ptr, score, year, name);
        g_kbo_league_ptr_cache_id  = league_id;
        g_kbo_league_ptr_cache_ptr = league_ptr;
        kbo_forget_league_ptr_miss(league_id);
        return league_ptr;
    }

    append_logf("KBO: named league ptr scan missed id=%u best_score=%d name=%s",
        league_id, score, name[0] != '\0' ? name : "(none)");
    kbo_remember_league_ptr_miss(league_id, now_ms);
    return 0;
}
