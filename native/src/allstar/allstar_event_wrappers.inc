/* All-Star event and voting preparation wrappers. */

__declspec(noinline) void ootp_kbo_prepare_allstar_events(uintptr_t league_ptr)
{
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    enable_kbo_allstar_flags(league_ptr, "make_allstar_game_events");
    kbo_perf_probe_record(
        "allstar_events_prepare",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
}

__declspec(noinline) void ootp_kbo_prepare_allstar_voting_begin(uintptr_t league_ptr, uintptr_t allstar_team_setup_ptr)
{
    (void)allstar_team_setup_ptr;

    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        return;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    if (!memory_range_readable((void*)league_ptr, layout.team_b_offset + sizeof(uint32_t))) {
        return;
    }

    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t old_allstar_game = league[layout.game_flag_offset];

    enable_kbo_allstar_flags(league_ptr, "allstar_voting_begin");

    LONG log_index = InterlockedIncrement(&g_allstar_voting_prepare_log_count);
    if (log_index <= 20) {
        append_logf(
            "prepared KBO all-star voting begin league=%p league_id=%u/%u year=%u game=%u->%u",
            league,
            league_id,
            fallback_league_id,
            league_year,
            old_allstar_game,
            league[layout.game_flag_offset]);
    }
}
