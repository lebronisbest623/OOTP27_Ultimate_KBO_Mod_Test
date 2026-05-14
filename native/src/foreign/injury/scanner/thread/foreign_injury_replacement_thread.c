#include "../foreign_injury_scanner_internal.h"

#include "../../../common/policy/foreign_player_policy.h"

DWORD WINAPI kbo_foreign_injury_replacement_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_foreign_injury_replacement_scan_once("foreign_injury_replacement_thread_start");
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_foreign_player_policy()->injury_replacement_scan_sleep_ms)) {
            break;
        }
        kbo_foreign_injury_replacement_scan_once("foreign_injury_replacement_thread");
    }
    InterlockedExchange(&g_kbo_foreign_injury_replacement_thread_started, 0);
    kbo_log_runtime_line("foreign injury replacement thread stopped");
    return 0;
}

void start_kbo_foreign_injury_replacement_thread(void)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        kbo_log_runtime_line("foreign injury replacement: disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_foreign_injury_replacement_thread_started, 1, 0) != 0) {
        return;
    }

    if (kbo_start_runtime_thread(
            kbo_foreign_injury_replacement_thread,
            NULL,
            "foreign injury replacement scanner")) {
        kbo_log_runtime_line("foreign injury replacement thread started");
    } else {
        InterlockedExchange(&g_kbo_foreign_injury_replacement_thread_started, 0);
    }
}
