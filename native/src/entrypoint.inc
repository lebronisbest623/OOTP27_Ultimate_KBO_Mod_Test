static HANDLE g_kbo_process_instance_mutex = NULL;

static DWORD WINAPI patch_thread(LPVOID parameter)
{
    append_log_line("KBOFix loaded");

    if (!read_kbo_localappdata_flag_file("enable_experimental_runtime_hooks.txt")) {
        append_log_line("KBOFix: experimental runtime hooks disabled; safe startup mode active");
        return 0;
    }

    start_kbo_hotkey_window_thread((HINSTANCE)parameter);

    if (!verify_ootp_build()) {
        append_log_line("KBOFix: build verification failed; no patches installed");
        return 0;
    }

    install_kbo_military_service_entry_patch();
    install_kbo_military_status_update_patch();
    install_kbo_player_team_signability_patch();
    install_kbo_ai_fa_signability_random_fallback_patch();
    start_kbo_foreign_waiver_scanner_thread();
    start_kbo_custom_event_monitor();
    start_kbo_military_days_tick_thread();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);

        char mutex_name[96] = {0};
        snprintf(
            mutex_name,
            sizeof(mutex_name),
            "Local\\OOTP_KBO_FIX_%lu",
            (unsigned long)GetCurrentProcessId());
        g_kbo_process_instance_mutex = CreateMutexA(NULL, TRUE, mutex_name);
        if (g_kbo_process_instance_mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(g_kbo_process_instance_mutex);
            g_kbo_process_instance_mutex = NULL;
            return TRUE;
        }

        HANDLE thread = CreateThread(NULL, 0, patch_thread, instance, 0, NULL);
        if (thread != NULL) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_kbo_process_instance_mutex != NULL) {
            CloseHandle(g_kbo_process_instance_mutex);
            g_kbo_process_instance_mutex = NULL;
        }
    }

    return TRUE;
}
