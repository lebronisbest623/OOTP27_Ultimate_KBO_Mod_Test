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
#include "../../../team/independent_acquisition/independent_acquisition_ai.h"
#include "../../schedules/independent/independent_team_acquisition_schedule.h"
#include "../calendar/custom_event_calendar_due.h"

static volatile LONG64 g_kbo_custom_event_schedule_deferred_log_ms = 0;
static volatile LONG64 g_kbo_custom_event_scan_deferred_log_ms = 0;

static int kbo_custom_event_monitor_should_log_throttled(volatile LONG64* last_log_ms)
{
    ULONGLONG now = GetTickCount64();
    LONG64 last = InterlockedCompareExchange64(last_log_ms, 0, 0);
    if (last > 0 && now >= (ULONGLONG)last && now - (ULONGLONG)last < 30000ull) {
        return 0;
    }
    return InterlockedCompareExchange64(last_log_ms, (LONG64)now, last) == last;
}

void kbo_custom_event_monitor_tick(
    uint32_t* last_scheduled_yyyymmdd,
    uint32_t* last_scanned_yyyymmdd,
    uint32_t* last_fa_comp_yyyymmdd,
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
        if (last_scanned_yyyymmdd != NULL) {
            *last_scanned_yyyymmdd = 0u;
        }
    }

    if (last_scheduled_yyyymmdd != NULL && today_yyyymmdd != *last_scheduled_yyyymmdd) {
        int due_result = kbo_process_custom_events_due_through(today_yyyymmdd, source);
        if (due_result >= 0) {
            *last_scheduled_yyyymmdd = today_yyyymmdd;
            if (last_scanned_yyyymmdd != NULL && due_result > 0) {
                *last_scanned_yyyymmdd = 0u;
            }
        } else if (kbo_custom_event_monitor_should_log_throttled(&g_kbo_custom_event_schedule_deferred_log_ms)) {
            kbo_log_runtimef(
                "KBO custom event schedule deferred reason=state_not_ready today=%u due_result=%d",
                today_yyyymmdd,
                due_result);
        }
    }
    int triggered = scan_kbo_custom_events_once(source);
    if (triggered >= 0 && last_scanned_yyyymmdd != NULL) {
        *last_scanned_yyyymmdd = today_yyyymmdd;
    } else if (triggered < 0 && kbo_custom_event_monitor_should_log_throttled(&g_kbo_custom_event_scan_deferred_log_ms)) {
        kbo_log_runtimef(
            "KBO custom event monitor scan deferred reason=state_not_ready today=%u",
            today_yyyymmdd);
    }

    if (last_fa_comp_yyyymmdd != NULL && today_yyyymmdd != *last_fa_comp_yyyymmdd) {
        kbo_process_due_fa_compensation_protected_lists(source);
        *last_fa_comp_yyyymmdd = today_yyyymmdd;
    }

    kbo_run_independent_team_acquisition_ai(source);
}

DWORD WINAPI kbo_custom_event_monitor_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO custom event monitor started");

    uint32_t last_scheduled_yyyymmdd = 0u;
    uint32_t last_scanned_yyyymmdd = 0u;
    uint32_t last_fa_comp_yyyymmdd = 0u;
    kbo_custom_event_monitor_tick(
        &last_scheduled_yyyymmdd,
        &last_scanned_yyyymmdd,
        &last_fa_comp_yyyymmdd,
        g_kbo_default_event_source);
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->custom_event_monitor_sleep_ms)) {
            break;
        }
        kbo_custom_event_monitor_tick(
            &last_scheduled_yyyymmdd,
            &last_scanned_yyyymmdd,
            &last_fa_comp_yyyymmdd,
            g_kbo_default_event_source);
    }
    InterlockedExchange(&g_kbo_custom_event_monitor_started, 0);
    kbo_log_runtime_line("KBO custom event monitor stopped");

    return 0;
}

int start_kbo_custom_event_monitor(void)
{
    if (!kbo_fix_enabled()) {
        kbo_log_runtime_line("KBO custom event monitor skipped reason=fix_disabled");
        return 0;
    }
    if (InterlockedCompareExchange(&g_kbo_custom_event_monitor_started, 1, 0) != 0) {
        return 1;
    }

    if (!kbo_start_runtime_thread(kbo_custom_event_monitor_thread, NULL, "custom event monitor")) {
        InterlockedExchange(&g_kbo_custom_event_monitor_started, 0);
        return 0;
    }
    return 1;
}
