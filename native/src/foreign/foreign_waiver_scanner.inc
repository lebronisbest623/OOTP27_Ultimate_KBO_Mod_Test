/* Foreign reserve-right background scanner thread. Included from native/src/foreign_waiver_ai.inc. */

static DWORD WINAPI kbo_foreign_waiver_scanner_thread(LPVOID parameter)
{
    (void)parameter;
    uint32_t tick = 0;
    for (;;) {
        Sleep(5000);
        tick++;
        process_foreign_waiver_commands();
        if (!kbo_is_foreign_waiver_negotiation_window_open()) {
            continue;
        }
        audit_foreign_roster_state("foreign_roster_pre_tick", 0);
        run_foreign_waiver_ai_core_once();
        if ((tick % 6u) == 0u) {
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
    if (!read_kbo_localappdata_flag_file("enable_foreign_waiver_background_scanner.txt")) {
        append_log_line("foreign waiver scanner thread disabled: enable_foreign_waiver_background_scanner.txt is off");
        return;
    }
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_waiver_scanner_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("foreign waiver scanner thread started");
    } else {
        InterlockedExchange(&started, 0);
        append_log_line("foreign waiver scanner thread failed to start");
    }
}
