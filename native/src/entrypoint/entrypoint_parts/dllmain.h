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
        kbo_request_runtime_threads_stop();
        if (g_kbo_process_instance_mutex != NULL) {
            CloseHandle(g_kbo_process_instance_mutex);
            g_kbo_process_instance_mutex = NULL;
        }
    }

    return TRUE;
}
