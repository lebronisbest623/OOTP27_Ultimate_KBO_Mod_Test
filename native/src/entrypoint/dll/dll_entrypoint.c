#include "../entrypoint_internal.h"

void start_kbo_full_runtime_marker_wait_thread(HINSTANCE instance)
{
    if (InterlockedCompareExchange(&g_kbo_full_runtime_marker_wait_started, 1, 0) != 0) {
        append_log_line("KBO full runtime marker guard thread already started");
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_full_runtime_marker_wait_thread, instance, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "full runtime marker wait");
    } else {
        InterlockedExchange(&g_kbo_full_runtime_marker_wait_started, 0);
        append_logf("KBO full runtime marker guard thread failed error=%lu", GetLastError());
    }
}

DWORD WINAPI patch_thread(LPVOID parameter)
{
    append_log_line("KBOFix loaded");
    append_log_line("KBOFix build includes retired unconditional all-star single-division gates");

    if (!read_kbo_localappdata_flag_file("enable_experimental_runtime_hooks.txt")) {
        append_log_line("KBOFix: experimental runtime hooks disabled; safe startup mode active");
        return 0;
    }

    int diagnostic_minimal_runtime = read_kbo_localappdata_flag_file("enable_kbo_diagnostic_minimal_runtime.txt");
    if (diagnostic_minimal_runtime) {
        append_log_line("KBO diagnostic minimal runtime enabled: F2 hub and runtime patches disabled");
    }

    if (!verify_ootp_build()) {
        append_log_line("KBOFix: build verification failed; no patches installed");
        return 0;
    }

    if (diagnostic_minimal_runtime) {
        append_log_line("KBO diagnostic minimal runtime: build verified, no runtime patches installed");
        return 0;
    }

    append_log_line("KBO all-star presave bootstrap retired: no all-star hooks are installed during league creation");

    if (read_kbo_localappdata_flag_file("disable_kbo_runtime_roster_marker_guard.txt")) {
        append_log_line("KBO runtime marker guard disabled by flag");
        install_kbo_full_runtime_after_roster_marker((HINSTANCE)parameter);
        return 0;
    }

    if (kbo_current_save_has_required_roster_marker("runtime_startup", 1)) {
        install_kbo_full_runtime_after_roster_marker((HINSTANCE)parameter);
    } else {
        start_kbo_full_runtime_marker_wait_thread((HINSTANCE)parameter);
    }
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
            kbo_register_runtime_thread(thread, "patch install");
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        if (reserved == NULL) {
            kbo_shutdown_runtime_threads(10000u);
        } else {
            kbo_request_runtime_threads_stop();
        }
        if (g_kbo_process_instance_mutex != NULL) {
            CloseHandle(g_kbo_process_instance_mutex);
            g_kbo_process_instance_mutex = NULL;
        }
    }

    return TRUE;
}

