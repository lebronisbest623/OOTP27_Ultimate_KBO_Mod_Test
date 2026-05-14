#include "../common/custom_events_common.h"
#include "custom_event_monitor.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"

void kbo_custom_event_monitor_tick(
    uint32_t* last_scheduled_yyyymmdd,
    uint32_t* last_scanned_yyyymmdd,
    const char* source)
{
    uint32_t today_yyyymmdd = 0u;
    if (!kbo_get_current_yyyymmdd(&today_yyyymmdd) || today_yyyymmdd == 0u) {
        return;
    }

    int offseason_transition_ready = kbo_custom_event_monitor_check_offseason_transition(
        today_yyyymmdd,
        source);
    if (offseason_transition_ready) {
        if (last_scheduled_yyyymmdd != NULL) {
            *last_scheduled_yyyymmdd = today_yyyymmdd;
        }
        if (last_scanned_yyyymmdd != NULL) {
            *last_scanned_yyyymmdd = 0u;
        }
    }

    if (last_scheduled_yyyymmdd != NULL && today_yyyymmdd != *last_scheduled_yyyymmdd) {
        int foreign_schedule = kbo_schedule_foreign_priority_custom_events(source);
        int asian_schedule = kbo_schedule_asian_games_custom_events(source);
        int cbt_schedule = kbo_schedule_cbt_custom_events(source);
        if (foreign_schedule >= 0 && asian_schedule >= 0 && cbt_schedule >= 0) {
            *last_scheduled_yyyymmdd = today_yyyymmdd;
        } else {
            append_logf(
                "KBO custom event schedule deferred reason=state_not_ready today=%u foreign=%d asian=%d cbt=%d",
                today_yyyymmdd,
                foreign_schedule,
                asian_schedule,
                cbt_schedule);
        }
    }
    if (last_scanned_yyyymmdd != NULL && today_yyyymmdd != *last_scanned_yyyymmdd) {
        int triggered = scan_kbo_custom_events_once(source);
        kbo_process_due_fa_compensation_protected_lists(source);
        if (triggered == 0) {
            *last_scanned_yyyymmdd = today_yyyymmdd;
        } else if (triggered < 0) {
            append_logf(
                "KBO custom event monitor scan deferred reason=state_not_ready today=%u",
                today_yyyymmdd);
        }
    }
}

DWORD WINAPI kbo_custom_event_monitor_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO custom event monitor started");

    uint32_t last_scheduled_yyyymmdd = 0u;
    uint32_t last_scanned_yyyymmdd = 0u;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->custom_event_monitor_sleep_ms)) {
            break;
        }
        kbo_custom_event_monitor_tick(
            &last_scheduled_yyyymmdd,
            &last_scanned_yyyymmdd,
            g_kbo_default_event_source);
    }
    InterlockedExchange(&g_kbo_custom_event_monitor_started, 0);
    append_log_line("KBO custom event monitor stopped");

    return 0;
}

int start_kbo_custom_event_monitor(void)
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
    kbo_register_runtime_thread(thread, "custom event monitor");
    return 1;
}
