#include "../internal/captain_selection_internal.h"

static int kbo_captain_current_yyyymmdd(uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }

    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return 0;
    }
    if (out_date != NULL) {
        *out_date = year * 10000u + month * 100u + day;
    }
    return 1;
}

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
        return 0;
    }

    uint32_t season = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    if (season < 1982u || season > 2200u) {
        season = date / 10000u;
    }
    if (season < 1982u || season > 2200u || phase != 2u) {
        return 0;
    }

    if (kbo_captain_selection_csv_exists(season)) {
        return 0;
    }

    LONG last_attempt = InterlockedCompareExchange(&g_kbo_captain_last_attempted_season, 0, 0);
    if (last_attempt == (LONG)season) {
        return 0;
    }
    InterlockedExchange(&g_kbo_captain_last_attempted_season, (LONG)season);

    KboCaptainSelectionRow rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(rows, 0, sizeof(rows));
    int selected_count = 0;
    int row_count = kbo_captain_select_for_preseason(
        date,
        season,
        league_id,
        rows,
        KBO_CAPTAIN_MAX_TEAMS,
        &selected_count);
    if (row_count <= 0 || selected_count <= 0) {
        append_logf(
            "KBO captain preseason selection skipped source=%s date=%u season=%u league_id=%u reason=no_selection rows=%d selected=%d",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            row_count,
            selected_count);
        InterlockedExchange(&g_kbo_captain_last_attempted_season, 0);
        return 0;
    }

    char csv_path[MAX_PATH] = {0};
    int wrote = kbo_captain_write_selection_csv(
        rows,
        row_count,
        source != NULL ? source : "captain_preseason_phase2",
        csv_path,
        sizeof(csv_path));
    if (wrote) {
        InterlockedExchange(&g_kbo_captain_last_attempted_season, 0);
        append_logf(
            "KBO captain preseason selection written source=%s date=%u season=%u league_id=%u rows=%d selected=%d csv=%s",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            row_count,
            selected_count,
            csv_path);
    } else {
        InterlockedExchange(&g_kbo_captain_last_attempted_season, 0);
    }
    return wrote;
}

DWORD WINAPI kbo_captain_preseason_selection_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO captain preseason selection thread started");

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(5000)) {
            break;
        }
        kbo_run_captain_preseason_selection_once("captain_preseason_thread");
    }

    InterlockedExchange(&g_kbo_captain_preseason_thread_started, 0);
    append_log_line("KBO captain preseason selection thread stopped");
    return 0;
}

void start_kbo_captain_preseason_selection_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_captain_preseason_thread_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_captain_preseason_selection_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "captain preseason selection");
    } else {
        InterlockedExchange(&g_kbo_captain_preseason_thread_started, 0);
        append_logf("KBO captain preseason selection thread failed error=%lu", GetLastError());
    }
}
