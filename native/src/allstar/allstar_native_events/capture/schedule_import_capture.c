#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/profiling/profiler.h"

#include <stdint.h>
#include <windows.h>

#include "../../flags/allstar_flags.h"
#include "../../allstar_league_context/allstar_league_context.h"
#include "../logging/native_event_logging.h"
#include "../schedule/schedule_dates.h"

__declspec(noinline) int ootp_kbo_capture_allstar_schedule_import_league(uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (league_ptr == 0) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.schedule_import_capture", 0);
    }

    int scoped_context = kbo_allstar_league_context_enabled(league_ptr);
    int kbo_schedule_context = kbo_allstar_league_uses_kbo_schedule_file(league_ptr);
    int raw_context = kbo_allstar_raw_kbo_league_context_enabled(league_ptr);
    if (!scoped_context && !kbo_schedule_context && !raw_context) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.schedule_import_capture", 0);
    }

    InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)league_ptr);
    if (scoped_context) {
        enable_kbo_allstar_flags(league_ptr, "schedule_import");
        seed_kbo_allstar_schedule_dates(league_ptr, "schedule_import");
    } else {
        enable_kbo_allstar_raw_flags_if_kbo_context(league_ptr, "schedule_import_raw");
        seed_kbo_allstar_schedule_dates(league_ptr, "schedule_import_raw");
    }

    static volatile LONG log_count = 0;
    LONG index = InterlockedIncrement(&log_count);
    if (index <= 12) {
        if (scoped_context) {
            log_kbo_allstar_native_event_state("schedule_import_capture", league_ptr, "schedule_import");
        } else {
            kbo_log_runtimef(
                "KBO all-star schedule import capture accepted raw KBO league=%p reason=%s",
                (void*)league_ptr,
                kbo_schedule_context ? "schedule_file_context" : "raw_kbo_context");
        }
    }

    KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.schedule_import_capture", 1);
}
