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
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../core/season/phase/capture/season_phase_capture.h"
#include "../../schedules/independent/independent_team_acquisition_schedule.h"
#include "../calendar/custom_event_calendar_due.h"

static volatile LONG64 g_kbo_custom_event_schedule_deferred_log_ms = 0;
static volatile LONG64 g_kbo_custom_event_scan_deferred_log_ms = 0;

#define KBO_CUSTOM_EVENT_MONITOR_PULSE_MS 100u
#define KBO_CUSTOM_EVENT_MONITOR_FAST_STABLE_TICKS 2

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
    KBO_PROFILE_BEGIN(profile_custom_event_monitor_tick);
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "custom_event_monitor")) {
        KBO_PROFILE_END(profile_custom_event_monitor_tick, "custom_event.monitor.save_pause_abort");
        return;
    }

    uint32_t today_yyyymmdd = 0u;
    if (!kbo_get_current_yyyymmdd(&today_yyyymmdd) || today_yyyymmdd == 0u) {
        KBO_PROFILE_END(profile_custom_event_monitor_tick, "custom_event.monitor.no_date");
        return;
    }

    KBO_PROFILE_BEGIN(profile_custom_event_monitor_transition);
    int offseason_transition_ready = kbo_custom_event_monitor_check_offseason_transition(
        today_yyyymmdd,
        source);
    KBO_PROFILE_END(profile_custom_event_monitor_transition, "custom_event.monitor.offseason_transition");
    if (offseason_transition_ready) {
        if (last_scanned_yyyymmdd != NULL) {
            *last_scanned_yyyymmdd = 0u;
        }
    }

    if (last_scheduled_yyyymmdd != NULL && today_yyyymmdd != *last_scheduled_yyyymmdd) {
        KBO_PROFILE_BEGIN(profile_custom_event_monitor_due);
        int due_result = kbo_process_custom_events_due_through(today_yyyymmdd, source);
        KBO_PROFILE_END(profile_custom_event_monitor_due, "custom_event.monitor.process_due");
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
    KBO_PROFILE_BEGIN(profile_custom_event_monitor_scan);
    int triggered = scan_kbo_custom_events_once(source);
    KBO_PROFILE_END(profile_custom_event_monitor_scan, "custom_event.monitor.scan_once");
    if (triggered >= 0 && last_scanned_yyyymmdd != NULL) {
        *last_scanned_yyyymmdd = today_yyyymmdd;
    } else if (triggered < 0 && kbo_custom_event_monitor_should_log_throttled(&g_kbo_custom_event_scan_deferred_log_ms)) {
        kbo_log_runtimef(
            "KBO custom event monitor scan deferred reason=state_not_ready today=%u",
            today_yyyymmdd);
    }

    if (last_fa_comp_yyyymmdd != NULL && today_yyyymmdd != *last_fa_comp_yyyymmdd) {
        KBO_PROFILE_BEGIN(profile_custom_event_monitor_fa_comp);
        kbo_process_due_fa_compensation_protected_lists(source);
        KBO_PROFILE_END(profile_custom_event_monitor_fa_comp, "custom_event.monitor.fa_comp_protected_lists");
        *last_fa_comp_yyyymmdd = today_yyyymmdd;
    }
    KBO_PROFILE_END(profile_custom_event_monitor_tick, "custom_event.monitor.tick");
}

DWORD WINAPI kbo_custom_event_monitor_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO custom event monitor started");

    uint32_t last_scheduled_yyyymmdd = 0u;
    uint32_t last_scanned_yyyymmdd = 0u;
    uint32_t last_fa_comp_yyyymmdd = 0u;
    uint32_t observed_yyyymmdd = 0u;
    uint32_t fast_processed_yyyymmdd = 0u;
    int observed_stable_ticks = 0;
    LONG last_phase_capture_sequence = InterlockedCompareExchange(
        &g_kbo_season_phase_capture_event_published_sequence,
        0,
        0);
    DWORD last_periodic_tick = GetTickCount();
    if (kbo_runtime_pause_for_save_if_needed("custom_event_monitor")) {
        kbo_custom_event_monitor_tick(
            &last_scheduled_yyyymmdd,
            &last_scanned_yyyymmdd,
            &last_fa_comp_yyyymmdd,
            g_kbo_default_event_source);
    }
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(KBO_CUSTOM_EVENT_MONITOR_PULSE_MS)) {
            break;
        }
        if (!kbo_runtime_pause_for_save_if_needed("custom_event_monitor")) {
            break;
        }

        uint32_t today_yyyymmdd = 0u;
        int has_date = kbo_get_current_yyyymmdd(&today_yyyymmdd) && today_yyyymmdd != 0u;
        if (has_date) {
            if (today_yyyymmdd != observed_yyyymmdd) {
                observed_yyyymmdd = today_yyyymmdd;
                observed_stable_ticks = 1;
            } else if (observed_stable_ticks < KBO_CUSTOM_EVENT_MONITOR_FAST_STABLE_TICKS) {
                observed_stable_ticks++;
            }

            if (today_yyyymmdd != fast_processed_yyyymmdd
                    && observed_stable_ticks >= KBO_CUSTOM_EVENT_MONITOR_FAST_STABLE_TICKS) {
                kbo_custom_event_monitor_tick(
                    &last_scheduled_yyyymmdd,
                    &last_scanned_yyyymmdd,
                    &last_fa_comp_yyyymmdd,
                    "custom_event_monitor_date_pulse");
                fast_processed_yyyymmdd = today_yyyymmdd;
                last_periodic_tick = GetTickCount();
                continue;
            }
        }

        LONG phase_capture_sequence = InterlockedCompareExchange(
            &g_kbo_season_phase_capture_event_published_sequence,
            0,
            0);
        if (has_date && phase_capture_sequence != last_phase_capture_sequence) {
            last_phase_capture_sequence = phase_capture_sequence;
            kbo_custom_event_monitor_tick(
                &last_scheduled_yyyymmdd,
                &last_scanned_yyyymmdd,
                &last_fa_comp_yyyymmdd,
                "custom_event_monitor_phase_pulse");
            last_periodic_tick = GetTickCount();
            continue;
        }

        DWORD now = GetTickCount();
        DWORD elapsed = now - last_periodic_tick;
        if (elapsed < (DWORD)kbo_runtime_tuning_policy()->custom_event_monitor_sleep_ms) {
            continue;
        }
        last_periodic_tick = now;
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
