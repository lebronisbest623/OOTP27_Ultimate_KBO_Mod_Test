static int kbo_run_captain_selection_maintenance_once(const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }

    uint32_t date = 0;
    if (!kbo_captain_current_yyyymmdd(&date)) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
    if (league_ptr == 0
            || !memory_range_readable(
                (void*)league_ptr,
                OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET + sizeof(uint32_t))) {
        return kbo_captain_run_seed_startup_without_league_ptr(date, league_id, source);
    }

    uint32_t league_season = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    uint32_t season = kbo_captain_effective_season(date, league_season);
    if (season < 1982u || season > 2200u) {
        return 0;
    }

    int calendar_recovery = kbo_captain_calendar_season_recovery_active(date, league_season, phase);
    int calendar_preseason = kbo_captain_calendar_preseason_window_active(date, league_season, phase);
    int seed_startup = calendar_preseason && kbo_captain_seed_available_for_season(season, league_id);
    int calendar_preseason_start = kbo_captain_calendar_preseason_start_active(
        date,
        league_season,
        phase,
        calendar_preseason);
    int csv_exists = kbo_captain_selection_csv_exists(season);
    kbo_captain_log_phase_observed(
        source,
        date,
        league_id,
        league_ptr,
        league_season,
        season,
        phase,
        csv_exists,
        calendar_recovery,
        calendar_preseason);

    static uint32_t last_thread_date = 0u;
    static uint32_t last_thread_league_id = 0u;
    static uint32_t last_thread_league_season = 0u;
    static uint32_t last_thread_effective_season = 0u;
    static uint8_t last_thread_phase = 0xffu;
    static int last_thread_csv_exists = -1;
    static int last_thread_calendar_recovery = -1;
    static int last_thread_calendar_preseason = -1;
    if (source != NULL
            && strcmp(source, "captain_selection_thread") == 0
            && date == last_thread_date
            && league_id == last_thread_league_id
            && league_season == last_thread_league_season
            && season == last_thread_effective_season
            && phase == last_thread_phase
            && csv_exists == last_thread_csv_exists
            && calendar_recovery == last_thread_calendar_recovery
            && calendar_preseason == last_thread_calendar_preseason) {
        return 0;
    }
    if (source != NULL && strcmp(source, "captain_selection_thread") == 0) {
        last_thread_date = date;
        last_thread_league_id = league_id;
        last_thread_league_season = league_season;
        last_thread_effective_season = season;
        last_thread_phase = phase;
        last_thread_csv_exists = csv_exists;
        last_thread_calendar_recovery = calendar_recovery;
        last_thread_calendar_preseason = calendar_preseason;
    }

    if (csv_exists) {
        KboCaptainSelectionRow summary_rows[KBO_CAPTAIN_MAX_TEAMS];
        memset(summary_rows, 0, sizeof(summary_rows));
        int summary_count = kbo_captain_load_selection_csv(season, summary_rows, KBO_CAPTAIN_MAX_TEAMS);
        if (summary_count > 0) {
            kbo_emit_captain_initial_selection_news(
                date,
                season,
                league_id,
                summary_rows,
                summary_count,
                source != NULL ? source : "captain_summary_maintenance");
        }
    }

    if (phase == 2u && !csv_exists) {
        return kbo_captain_write_missing_selection_csv(
            date,
            season,
            league_id,
            phase,
            source != NULL ? source : "captain_preseason_start");
    }
    if ((seed_startup || calendar_preseason_start) && !csv_exists) {
        return kbo_captain_write_missing_selection_csv(
            date,
            season,
            league_id,
            phase,
            source != NULL
                ? source
                : (seed_startup ? "captain_seed_startup" : "captain_calendar_preseason_start"));
    }
    if (phase == 3u) {
        if (!csv_exists) {
            return kbo_captain_write_missing_selection_csv(
                date,
                season,
                league_id,
                phase,
                source != NULL ? source : "captain_missed_preseason_recovery");
        }
        return kbo_run_captain_inseason_repair_once(
            date,
            season,
            league_id,
            source != NULL ? source : "captain_inseason_thread");
    }
    if (calendar_recovery) {
        if (!csv_exists) {
            return 0;
        }
        return kbo_run_captain_inseason_repair_once(
            date,
            season,
            league_id,
            source != NULL ? source : "captain_calendar_year_repair");
    }
    return 0;
}
