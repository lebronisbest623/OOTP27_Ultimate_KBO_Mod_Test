static DWORD WINAPI kbo_full_runtime_marker_wait_thread(LPVOID parameter)
{
    HINSTANCE instance = (HINSTANCE)parameter;
    append_log_line("KBO full runtime marker guard thread started");

    for (int attempt = 1; attempt <= 450; attempt++) {
        int log_detail = attempt == 1 || attempt == 5 || attempt == 15 || attempt % 30 == 0;
        if (kbo_current_save_has_required_roster_marker("runtime_marker_wait", log_detail)) {
            install_kbo_full_runtime_after_roster_marker(instance);
            return 0;
        }
        Sleep(2000);
    }

    append_log_line("KBO full runtime marker guard gave up: required roster marker was not found");
    return 0;
}

static void start_kbo_full_runtime_marker_wait_thread(HINSTANCE instance)
{
    if (InterlockedCompareExchange(&g_kbo_full_runtime_marker_wait_started, 1, 0) != 0) {
        append_log_line("KBO full runtime marker guard thread already started");
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_full_runtime_marker_wait_thread, instance, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_kbo_full_runtime_marker_wait_started, 0);
        append_logf("KBO full runtime marker guard thread failed error=%lu", GetLastError());
    }
}
