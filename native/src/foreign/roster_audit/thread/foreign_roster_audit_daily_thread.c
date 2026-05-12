#include "../internal/foreign_roster_audit_internal.h"
#include "../../../team/add_player_guard/team_add_player_guard_ai_roster.h"

DWORD WINAPI kbo_foreign_roster_daily_audit_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("foreign roster daily audit thread started");

    uint32_t last_audit_date = 0u;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(5000)) {
            break;
        }

        uint32_t today = 0u;
        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_yyyymmdd(&today)
                || !kbo_get_current_save_path(save_path, sizeof(save_path))) {
            continue;
        }

        if (today == 0u || today == last_audit_date) {
            continue;
        }

        kbo_run_foreign_ai_roster_daily_callup("foreign_roster_daily_date_change");
        audit_foreign_roster_state("foreign_roster_daily_date_change", 1);
        last_audit_date = today;
    }

    InterlockedExchange(&g_kbo_foreign_roster_daily_audit_started, 0);
    append_log_line("foreign roster daily audit thread stopped");
    return 0;
}

void start_kbo_foreign_roster_daily_audit_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_foreign_roster_daily_audit_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_roster_daily_audit_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "foreign roster daily audit");
    } else {
        InterlockedExchange(&g_kbo_foreign_roster_daily_audit_started, 0);
        append_logf("foreign roster daily audit thread failed error=%lu", GetLastError());
    }
}
