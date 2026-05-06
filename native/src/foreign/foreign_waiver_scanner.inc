/* Foreign reserve-right background scanner thread. Included from native/src/foreign_waiver_ai.inc. */

static DWORD WINAPI kbo_foreign_waiver_scanner_thread(LPVOID parameter)
{
    (void)parameter;
    uint32_t tick = 0;
    uint32_t last_ai_run_date = 0u;
    for (;;) {
        Sleep(5000);
        tick++;
        uint32_t today = 0u;
        char readiness_path[MAX_PATH] = {0};
        if (!kbo_get_current_yyyymmdd(&today)
                || !kbo_get_save_scoped_data_file("foreign_waiver_commands.txt", readiness_path, sizeof(readiness_path))) {
            static LONG waiting_logged = 0;
            if (InterlockedCompareExchange(&waiting_logged, 1, 0) == 0) {
                append_log_line("foreign waiver worker waiting: save path/date not ready");
            }
            continue;
        }

        process_foreign_waiver_commands();
        if (!kbo_is_foreign_waiver_negotiation_window_open()) {
            continue;
        }

        int background_scanner_enabled = read_kbo_localappdata_flag_file("enable_foreign_waiver_background_scanner.txt");
        if (background_scanner_enabled) {
            audit_foreign_roster_state("foreign_roster_pre_tick", 0);
        }
        if (today != last_ai_run_date) {
            run_foreign_waiver_ai_core_once();
            last_ai_run_date = today;
        }
        if (background_scanner_enabled && (tick % 6u) == 0u) {
            audit_foreign_roster_state("foreign_roster_post_tick", 1);
            write_foreign_waiver_candidates("foreign_waiver_scanner");
        }
    }
    return 0;
}

static void start_kbo_foreign_waiver_scanner_thread(void)
{
    static LONG started = 0;
    if (!kbo_foreign_waiver_ai_enabled()) {
        return;
    }
    int background_scanner_enabled = read_kbo_localappdata_flag_file("enable_foreign_waiver_background_scanner.txt");
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_waiver_scanner_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        if (background_scanner_enabled) {
            append_log_line("foreign waiver scanner thread started");
        } else {
            append_log_line("foreign waiver lightweight retain worker started; candidate scanner disabled");
        }
    } else {
        InterlockedExchange(&started, 0);
        append_log_line("foreign waiver scanner thread failed to start");
    }
}
