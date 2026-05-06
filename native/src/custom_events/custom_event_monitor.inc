/* Custom-event monitor thread startup and loop. Included from native/src/custom_events.inc. */

static DWORD WINAPI kbo_custom_event_monitor_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO custom event monitor started");

    uint32_t last_scheduled_yyyymmdd = 0u;
    uint32_t last_scanned_yyyymmdd = 0u;
    for (;;) {
        Sleep(5000);

        uint32_t today_yyyymmdd = 0u;
        if (!kbo_get_current_yyyymmdd(&today_yyyymmdd) || today_yyyymmdd == 0u) {
            kbo_schedule_foreign_priority_custom_events(g_kbo_default_event_source);
            kbo_schedule_asian_games_custom_events(g_kbo_default_event_source);
            continue;
        }

        if (today_yyyymmdd != last_scheduled_yyyymmdd) {
            int foreign_schedule = kbo_schedule_foreign_priority_custom_events(g_kbo_default_event_source);
            int asian_schedule = kbo_schedule_asian_games_custom_events(g_kbo_default_event_source);
            if (foreign_schedule >= 0 && asian_schedule >= 0) {
                last_scheduled_yyyymmdd = today_yyyymmdd;
            } else {
                append_logf(
                    "KBO custom event schedule deferred reason=state_not_ready today=%u foreign=%d asian=%d",
                    today_yyyymmdd,
                    foreign_schedule,
                    asian_schedule);
            }
        }
        if (today_yyyymmdd != last_scanned_yyyymmdd) {
            int triggered = scan_kbo_custom_events_once("custom_event_monitor");
            if (triggered == 0) {
                last_scanned_yyyymmdd = today_yyyymmdd;
            } else if (triggered < 0) {
                append_logf(
                    "KBO custom event monitor scan deferred reason=state_not_ready today=%u",
                    today_yyyymmdd);
            }
        }
    }

    return 0;
}

static int start_kbo_custom_event_monitor(void)
{
    if (!kbo_fix_enabled()) {
        append_log_line("KBO custom event monitor skipped reason=fix_disabled");
        return 0;
    }
    if (InterlockedCompareExchange(&g_kbo_custom_event_monitor_started, 1, 0) != 0) {
        return 1;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_custom_event_monitor_thread, NULL, 0, NULL);
    if (thread == NULL) {
        InterlockedExchange(&g_kbo_custom_event_monitor_started, 0);
        append_logf("KBO custom event monitor skipped reason=create_thread_failed error=%lu", GetLastError());
        return 0;
    }
    CloseHandle(thread);
    return 1;
}

