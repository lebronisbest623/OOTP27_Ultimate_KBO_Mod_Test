#include "salary_snapshot_thread.h"

#include <stdint.h>
#include <windows.h>

#include "../../bootstrap/profiling/profiler.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../paths/salary_snapshot_paths_dates.h"
#include "../state/salary_snapshot_state.h"
#include "../capture/salary_snapshot_write_capture.h"

static DWORD WINAPI kbo_fa_salary_snapshot_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO FA salary opening-day snapshot thread started");

    uint32_t last_log_date = 0u;
    uint32_t last_log_opening_day = 0u;
    uint32_t captured_season = 0u;
    uint32_t cached_message_date = 0u;
    int cached_message_found = 0;
    DWORD cached_message_checked_ms = 0u;
    uint32_t quiet_opening_unavailable_year = 0u;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(KBO_FA_SALARY_SNAPSHOT_THREAD_SLEEP_MS)) {
            break;
        }
        LARGE_INTEGER profile_snapshot_thread_tick = {0};
        int profile_snapshot_thread_tick_active = kbo_profiler_begin(&profile_snapshot_thread_tick);
        if (!kbo_fix_enabled()) {
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.disabled_tick", &profile_snapshot_thread_tick);
            }
            continue;
        }

        uint32_t year = 0;
        uint32_t month = 0;
        uint32_t day = 0;
        if (!kbo_current_date_is_valid(&year, &month, &day)) {
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.no_date", &profile_snapshot_thread_tick);
            }
            continue;
        }
        uint32_t date = year * 10000u + month * 100u + day;

        uint32_t league_id = kbo_resolve_kbo_league_id();
        uintptr_t league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
        uint32_t opening_day = 0u;
        if (league_ptr == 0 || !kbo_fa_salary_snapshot_read_opening_day(league_ptr, &opening_day)) {
            if (kbo_fa_salary_snapshot_load_schedule_opening_day(year, &opening_day)) {
                if (date != last_log_date || opening_day != last_log_opening_day) {
                    append_logf(
                        "KBO FA salary snapshot opening day fallback date=%u opening_day=%u league=%u source=schedule",
                        date,
                        opening_day,
                        league_id);
                    last_log_date = date;
                    last_log_opening_day = opening_day;
                }
            }
        }
        if (opening_day == 0u) {
            DWORD now_ms = GetTickCount();
            if (cached_message_date != date
                    && (cached_message_checked_ms == 0u || now_ms - cached_message_checked_ms >= 30000u)) {
                cached_message_date = date;
                cached_message_checked_ms = now_ms;
                cached_message_found = kbo_fa_salary_snapshot_today_has_opening_day_message(date);
            }

            if (!cached_message_found) {
                if (quiet_opening_unavailable_year != year && date != last_log_date) {
                    append_logf("KBO FA salary snapshot waiting date=%u league=%u reason=opening_day_memory_unavailable", date, league_id);
                    last_log_date = date;
                    if (month >= 5u) {
                        quiet_opening_unavailable_year = year;
                    }
                }
                if (profile_snapshot_thread_tick_active) {
                    kbo_profiler_end("fa_salary_snapshot.thread.opening_day_unavailable", &profile_snapshot_thread_tick);
                }
                continue;
            }
            opening_day = date;
        }

        int in_opening_window = kbo_fa_salary_snapshot_current_date_in_opening_window(date, opening_day);
        int late_missing_snapshot_backfill =
            !in_opening_window
            && opening_day / 10000u == year
            && date > opening_day
            && !kbo_fa_salary_snapshot_file_exists(year);
        if (!in_opening_window && !late_missing_snapshot_backfill) {
            if (date != last_log_date || opening_day != last_log_opening_day) {
                append_logf(
                    "KBO FA salary snapshot waiting date=%u opening_day=%u league=%u",
                    date,
                    opening_day,
                    league_id);
                last_log_date = date;
                last_log_opening_day = opening_day;
            }
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.outside_opening_window", &profile_snapshot_thread_tick);
            }
            continue;
        }
        if (late_missing_snapshot_backfill) {
            append_logf(
                "KBO FA salary snapshot late backfill date=%u opening_day=%u league=%u reason=missing_opening_day_snapshot",
                date,
                opening_day,
                league_id);
        }

        if (captured_season == year || kbo_fa_salary_snapshot_file_exists(year)) {
            captured_season = year;
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.already_captured", &profile_snapshot_thread_tick);
            }
            continue;
        }

        if (kbo_capture_fa_salary_opening_day_snapshot(
                late_missing_snapshot_backfill ? "opening_day_thread_late_backfill" : "opening_day_thread",
                date,
                year,
                opening_day,
                league_id)) {
            captured_season = year;
        }
        if (profile_snapshot_thread_tick_active) {
            kbo_profiler_end("fa_salary_snapshot.thread.capture_attempt", &profile_snapshot_thread_tick);
        }
    }
    InterlockedExchange(&g_kbo_fa_salary_snapshot_thread_started, 0);
    append_log_line("KBO FA salary opening-day snapshot thread stopped");

    return 0;
}

void start_kbo_fa_salary_snapshot_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_thread_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_fa_salary_snapshot_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "FA salary snapshot");
    } else {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_thread_started, 0);
        append_logf("KBO FA salary opening-day snapshot thread failed to start gle=%lu", GetLastError());
    }
}
