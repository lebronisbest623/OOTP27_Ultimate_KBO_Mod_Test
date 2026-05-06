static void kbo_run_fa_requalification_once(const char* source)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_no_date_log_count);
        if (slot <= 20 || (slot % 60) == 0) {
            append_logf(
                "KBO FA requalification skipped source=%s reason=current_date_unavailable count=%ld",
                source != NULL ? source : "",
                slot);
        }
        return;
    }
    uint32_t current_year = 0u;
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id != 0u) {
        current_year = kbo_find_league_year_from_id(kbo_league_id);
    }
    if (current_year < 1982u || current_year > 2200u) {
        current_year = today / 10000u;
    }
    if (current_year < 1982u || current_year > 2200u) {
        append_logf(
            "KBO FA requalification skipped source=%s reason=invalid_current_year today=%u current_year=%u",
            source != NULL ? source : "",
            today,
            current_year);
        return;
    }

    KboFaRequalificationRecord records[KBO_FA_REQUALIFICATION_MAX];
    kbo_lock_fa_requalification_records();
    int count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    kbo_unlock_fa_requalification_records();
    if (count <= 0) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_no_records_log_count);
        if (g_kbo_fa_requalification_last_no_records_date != today || slot <= 5 || (slot % 60) == 0) {
            char path[MAX_PATH] = {0};
            get_kbo_fa_requalification_path(path, sizeof(path));
            append_logf(
                "KBO FA requalification pass source=%s records=0 restored=0 today=%u path=%s",
                source != NULL ? source : "",
                today,
                path);
            g_kbo_fa_requalification_last_no_records_date = today;
        }
        return;
    }
    int restored = 0;
    for (int i = 0; i < count; i++) {
        restored += kbo_restore_fa_requalification_team_control(&records[i], current_year, source);
    }
    append_logf("KBO FA requalification pass source=%s records=%d restored=%d today=%u", source != NULL ? source : "", count, restored, today);
}

static DWORD WINAPI kbo_fa_requalification_thread(LPVOID parameter)
{
    (void)parameter;
    for (;;) {
        Sleep(5000);
        kbo_run_fa_requalification_once("fa_requalification_monitor");
    }
    return 0;
}

static void start_kbo_fa_requalification_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_fa_requalification_thread_started, 1, 0) != 0) {
        return;
    }
    HANDLE thread = CreateThread(NULL, 0, kbo_fa_requalification_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        append_log_line("KBO FA requalification monitor thread started");
    } else {
        InterlockedExchange(&g_kbo_fa_requalification_thread_started, 0);
        append_log_line("KBO FA requalification monitor thread failed to start");
    }
}
