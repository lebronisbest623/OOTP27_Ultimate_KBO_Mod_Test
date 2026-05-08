static void kbo_intl_established_fa_postscan_schedule(
    int32_t original_count,
    int32_t expected_count,
    int multiplier,
    uint32_t primary_league_id,
    uint32_t fallback_league_id)
{
    if (expected_count <= 0) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t max_player_id = 0u;
    int vector_ready = find_kbo_global_player_vector(&player_vector, &player_count, NULL);
    if (vector_ready) {
        for (int32_t i = 0; i < player_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            if (player_id > max_player_id) {
                max_player_id = player_id;
            }
        }
    }

    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);

    LONG batch_id = InterlockedIncrement(&g_kbo_intl_established_fa_postscan.batch_id);
    g_kbo_intl_established_fa_postscan.before_count = vector_ready ? player_count : 0;
    g_kbo_intl_established_fa_postscan.before_max_player_id = max_player_id;
    g_kbo_intl_established_fa_postscan.original_count = original_count;
    g_kbo_intl_established_fa_postscan.expected_count = expected_count;
    g_kbo_intl_established_fa_postscan.multiplier = multiplier;
    g_kbo_intl_established_fa_postscan.primary_league_id = primary_league_id;
    g_kbo_intl_established_fa_postscan.fallback_league_id = fallback_league_id;
    g_kbo_intl_established_fa_postscan.scheduled_date = today;
    g_kbo_intl_established_fa_postscan.due_tick = GetTickCount64() + KBO_INTL_ESTABLISHED_FA_POSTSCAN_DELAY_MS;
    g_kbo_intl_established_fa_postscan.attempts = 0;
    InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);

    append_logf(
        "international established FA postscan scheduled batch=%ld league_id=%u/%u before_count=%d before_max_player=%u original=%d multiplier=%d expected=%d date=%u vector_ready=%d",
        batch_id,
        primary_league_id,
        fallback_league_id,
        vector_ready ? player_count : 0,
        max_player_id,
        original_count,
        multiplier,
        expected_count,
        today,
        vector_ready ? 1 : 0);
}
