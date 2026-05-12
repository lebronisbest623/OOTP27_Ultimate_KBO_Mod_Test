/* All-Star startup retry thread. */

#include "allstar_flags.h"

#include <windows.h>

#include "../allstar_league_context/allstar_league_context.h"
#include "../allstar_native_events/generation/event_generation.h"
#include "../allstar_native_events/schedule/schedule_dates.h"
#include "../team_patch/allstar_team_patch.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"

static DWORD WINAPI kbo_allstar_force_retry_thread(LPVOID parameter)
{
    (void)parameter;

    for (int attempt = 1; attempt <= 180; attempt++) {
        uint32_t league_id = kbo_get_foreign_waiver_league_id();
        if (league_id == 0u) {
            league_id = kbo_resolve_kbo_league_id();
        }

        uintptr_t captured = (uintptr_t)InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
            NULL,
            NULL);
        if (captured != 0
                && enable_kbo_allstar_raw_flags_if_kbo_context(captured, "startup_retry_captured_raw")) {
            append_logf(
                "KBO all-star force retry using captured raw league attempt=%d league_id=%u league=%p",
                attempt,
                league_id,
                (void*)captured);
            seed_kbo_allstar_schedule_dates(captured, "startup_retry_captured_raw");
            if (run_kbo_allstar_native_event_generation(captured, "startup_retry_captured_raw")) {
                append_log_line("KBO all-star force retry completed by captured raw league");
                return 0;
            }
        }

        patch_kbo_allstar_team_names_for_league_id(league_id, "startup_retry");
        patch_kbo_allstar_team_names_for_known_exhibition_teams("startup_retry");

        uintptr_t league_ptr = kbo_find_allstar_league_ptr(league_id);
        if (league_ptr != 0) {
            append_logf("KBO all-star force retry found league attempt=%d league_id=%u league=%p", attempt, league_id, (void*)league_ptr);
            enable_kbo_allstar_flags(league_ptr, "startup_retry");
            InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)league_ptr);
            seed_kbo_allstar_schedule_dates(league_ptr, "startup_retry");
            run_kbo_allstar_native_event_generation(league_ptr, "startup_retry");
            append_log_line("KBO all-star force retry completed");
            return 0;
        }

        league_ptr = find_kbo_allstar_core_fallback_league(league_id);
        if (league_ptr != 0
                && enable_kbo_allstar_flags_for_core_league(league_ptr, league_id, "startup_retry_core_fallback")) {
            append_logf(
                "KBO all-star force retry found league by core fallback attempt=%d league_id=%u league=%p",
                attempt,
                league_id,
                (void*)league_ptr);
            InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)league_ptr);
            seed_kbo_allstar_schedule_dates(league_ptr, "startup_retry_core_fallback");
            run_kbo_allstar_native_event_generation(league_ptr, "startup_retry_core_fallback");
            append_log_line("KBO all-star force retry completed by core fallback");
            return 0;
        }

        if (!kbo_runtime_sleep_should_continue(1000)) {
            break;
        }
    }

    append_log_line("KBO all-star force retry gave up: league not found after 180 attempts");
    return 0;
}

void start_kbo_allstar_force_retry_thread(void)
{
    static LONG started = 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_allstar_force_retry_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "all-star force retry");
        append_log_line("KBO all-star force retry thread started");
    } else {
        InterlockedExchange(&started, 0);
        append_log_line("KBO all-star force retry thread failed to start");
    }
}
