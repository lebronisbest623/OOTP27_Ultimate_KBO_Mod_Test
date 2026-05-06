/* All-Star league flag enforcement and startup retry. */

static void enable_kbo_allstar_flags(uintptr_t league_ptr, const char* source)
{
    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        return;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))) {
        return;
    }

    uint8_t old_rules_allstar = league[layout.rules_flag_offset];
    uint8_t old_allstar_game = league[layout.game_flag_offset];
    uint8_t old_auto_schedule_allstar = league[layout.auto_schedule_offset];

    league[layout.rules_flag_offset] = 1;
    league[layout.game_flag_offset] = 1;
    league[layout.auto_schedule_offset] = 1;
    ensure_kbo_allstar_team_ids(league_ptr, source);

    if (old_rules_allstar != 1 || old_allstar_game != 1 || old_auto_schedule_allstar != 1) {
        uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        append_logf(
            "enabled KBO all-star flags source=%s league=%p year=%u rules 0x%x=%u->1 game 0x%x=%u->1 auto 0x%x=%u->1",
            source,
            league,
            league_year,
            layout.rules_flag_offset,
            old_rules_allstar,
            layout.game_flag_offset,
            old_allstar_game,
            layout.auto_schedule_offset,
            old_auto_schedule_allstar);
    }
}

__declspec(noinline) void ootp_kbo_enable_allstar_setting(uintptr_t league_ptr)
{
    enable_kbo_allstar_flags(league_ptr, "allstar_settings_ui");
}

static void force_kbo_allstar_flags_for_configured_league(const char* source)
{
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    uintptr_t league_ptr = kbo_find_allstar_league_ptr(league_id);
    if (league_ptr == 0) {
        append_logf("KBO all-star force flags skipped source=%s league_id=%u reason=league_not_found", source != NULL ? source : "", league_id);
        return;
    }
    enable_kbo_allstar_flags(league_ptr, source != NULL ? source : "allstar_force_flags");
}

static DWORD WINAPI kbo_allstar_force_retry_thread(LPVOID parameter)
{
    (void)parameter;

    for (int attempt = 1; attempt <= 180; attempt++) {
        uint32_t league_id = kbo_get_foreign_waiver_league_id();
        if (league_id == 0u) {
            league_id = kbo_resolve_kbo_league_id();
        }

        patch_kbo_allstar_team_names_for_league_id(league_id, "startup_retry");
        patch_kbo_allstar_team_names_for_known_exhibition_teams("startup_retry");

        uintptr_t league_ptr = kbo_find_allstar_league_ptr(league_id);
        if (league_ptr != 0) {
            append_logf("KBO all-star force retry found league attempt=%d league_id=%u league=%p", attempt, league_id, (void*)league_ptr);
            enable_kbo_allstar_flags(league_ptr, "startup_retry");
            if (read_kbo_localappdata_flag_file("enable_single_division_allstar_events.txt")
                    && run_kbo_allstar_native_event_generation(league_ptr, "startup_retry")) {
                return 0;
            }
        }

        Sleep(1000);
    }

    append_log_line("KBO all-star force retry gave up: league not found after 180 attempts");
    return 0;
}

static void start_kbo_allstar_force_retry_thread(void)
{
    static LONG started = 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_allstar_force_retry_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("KBO all-star force retry thread started");
    } else {
        InterlockedExchange(&started, 0);
        append_log_line("KBO all-star force retry thread failed to start");
    }
}
