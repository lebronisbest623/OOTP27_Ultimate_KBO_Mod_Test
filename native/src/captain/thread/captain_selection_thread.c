#include "../internal/captain_selection_internal.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"

int kbo_run_captain_preseason_selection_once(const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }

    uint32_t date = 0;
    if (!kbo_captain_current_yyyymmdd(&date)) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
    if (league_ptr == 0
            || !memory_range_readable(
                (void*)league_ptr,
                OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET + sizeof(uint32_t))) {
        return kbo_captain_run_seed_startup_without_league_ptr(date, league_id, source);
    }

    uint32_t league_season = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    uint32_t season = kbo_captain_effective_season(date, league_season);
    int calendar_preseason = kbo_captain_calendar_preseason_window_active(date, league_season, phase);
    int seed_startup = calendar_preseason && kbo_captain_seed_available_for_season(season, league_id);
    int calendar_preseason_start = kbo_captain_calendar_preseason_start_active(
        date,
        league_season,
        phase,
        calendar_preseason);
    if (season < 1982u || season > 2200u || (phase != 2u && !seed_startup && !calendar_preseason_start)) {
        return 0;
    }

    return kbo_captain_write_missing_selection_csv(
        date,
        season,
        league_id,
        phase,
        source != NULL
            ? source
            : (seed_startup
                ? "captain_seed_startup"
                : (calendar_preseason_start ? "captain_calendar_preseason_start" : "captain_preseason_phase2")));
}

DWORD WINAPI kbo_captain_preseason_selection_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO captain selection maintenance thread started");

    while (kbo_runtime_threads_should_continue()) {
        kbo_run_captain_selection_maintenance_once("captain_selection_thread");
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->captain_selection_thread_sleep_ms)) {
            break;
        }
    }

    InterlockedExchange(&g_kbo_captain_preseason_thread_started, 0);
    append_log_line("KBO captain selection maintenance thread stopped");
    return 0;
}

void start_kbo_captain_preseason_selection_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_captain_preseason_thread_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(kbo_captain_preseason_selection_thread, NULL, "captain selection maintenance")) {
        InterlockedExchange(&g_kbo_captain_preseason_thread_started, 0);
    }
}
