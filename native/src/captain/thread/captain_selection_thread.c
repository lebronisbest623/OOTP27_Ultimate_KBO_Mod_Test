#include "../internal/captain_selection_internal.h"

static const char* kbo_captain_phase_label(uint8_t phase)
{
    switch (phase) {
    case 0: return "offseason_or_reset";
    case 1: return "offseason_started";
    case 2: return "preseason_or_spring";
    case 3: return "regular_season";
    case 4: return "postseason_or_transition";
    default: return "unknown";
    }
}

static void kbo_captain_log_phase_observed(
    const char* source,
    uint32_t date,
    uint32_t league_id,
    uintptr_t league_ptr,
    uint32_t league_season,
    uint32_t effective_season,
    uint8_t phase,
    int csv_exists,
    int calendar_recovery,
    int calendar_preseason)
{
    static uintptr_t last_league_ptr = 0u;
    static uint32_t last_date = 0xffffffffu;
    static uint32_t last_league_id = 0xffffffffu;
    static uint32_t last_league_season = 0xffffffffu;
    static uint32_t last_effective_season = 0xffffffffu;
    static uint8_t last_phase = 0xffu;
    static int last_csv_exists = -1;
    static int last_calendar_recovery = -1;
    static int last_calendar_preseason = -1;

    if (league_ptr == last_league_ptr
            && date == last_date
            && league_id == last_league_id
            && league_season == last_league_season
            && effective_season == last_effective_season
            && phase == last_phase
            && csv_exists == last_csv_exists
            && calendar_recovery == last_calendar_recovery
            && calendar_preseason == last_calendar_preseason) {
        return;
    }

    append_logf(
        "KBO captain phase observed source=%s date=%u league_id=%u league=%p league_season=%u effective_season=%u phase=%u label=%s csv_exists=%d calendar_recovery=%d calendar_preseason=%d",
        source != NULL ? source : "",
        date,
        league_id,
        (void*)league_ptr,
        league_season,
        effective_season,
        (unsigned)phase,
        kbo_captain_phase_label(phase),
        csv_exists,
        calendar_recovery,
        calendar_preseason);

    last_league_ptr = league_ptr;
    last_date = date;
    last_league_id = league_id;
    last_league_season = league_season;
    last_effective_season = effective_season;
    last_phase = phase;
    last_csv_exists = csv_exists;
    last_calendar_recovery = calendar_recovery;
    last_calendar_preseason = calendar_preseason;
}

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

static int kbo_captain_find_row_index_by_team(
    const KboCaptainSelectionRow* rows,
    int row_count,
    uint32_t team_id)
{
    if (rows == NULL || row_count <= 0 || team_id == 0u) {
        return -1;
    }
    for (int i = 0; i < row_count; i++) {
        if (rows[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}

static uint8_t* kbo_captain_find_player_by_id(uint32_t player_id, int* out_vector_available)
{
    if (out_vector_available != NULL) {
        *out_vector_available = 0;
    }
    if (player_id == 0u) {
        return NULL;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }
    if (player_vector == 0u || player_count <= 0 || player_count > 200000
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return NULL;
    }
    if (out_vector_available != NULL) {
        *out_vector_available = 1;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}

static int kbo_captain_existing_row_still_with_team(const KboCaptainSelectionRow* row)
{
    if (row == NULL || row->team_id == 0u || row->player_id == 0u) {
        return 0;
    }

    int vector_available = 0;
    uint8_t* player = kbo_captain_find_player_by_id(row->player_id, &vector_available);
    if (!vector_available) {
        return 1;
    }
    if (player == NULL) {
        return 0;
    }
    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        return 0;
    }
    return kbo_player_current_assignment_matches_team_or_affiliate(player, row->team_id);
}

static int kbo_captain_current_rows_need_inseason_repair(
    uint32_t league_id,
    const KboCaptainSelectionRow* current_rows,
    int current_count,
    uint32_t* out_team_ids,
    int* out_team_count,
    int* out_missing_count,
    int* out_departed_count)
{
    if (out_team_count != NULL) { *out_team_count = 0; }
    if (out_missing_count != NULL) { *out_missing_count = 0; }
    if (out_departed_count != NULL) { *out_departed_count = 0; }
    if (out_team_ids == NULL || league_id == 0u) {
        return 0;
    }

    int scanned_teams = 0;
    int unreadable_teams = 0;
    int team_count = collect_kbo_league_team_ids(
        league_id,
        out_team_ids,
        KBO_CAPTAIN_MAX_TEAMS,
        &scanned_teams,
        &unreadable_teams);
    if (team_count <= 0) {
        return 0;
    }
    if (out_team_count != NULL) {
        *out_team_count = team_count;
    }

    int missing_count = 0;
    int departed_count = 0;
    for (int i = 0; i < team_count; i++) {
        int index = kbo_captain_find_row_index_by_team(current_rows, current_count, out_team_ids[i]);
        if (index < 0) {
            missing_count++;
            continue;
        }
        if (!kbo_captain_existing_row_still_with_team(&current_rows[index])) {
            departed_count++;
        }
    }
    if (out_missing_count != NULL) { *out_missing_count = missing_count; }
    if (out_departed_count != NULL) { *out_departed_count = departed_count; }
    return missing_count > 0 || departed_count > 0;
}

static int kbo_captain_write_initial_selection(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const char* source)
{
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
            "KBO captain selection skipped source=%s date=%u season=%u league_id=%u reason=no_selection rows=%d selected=%d",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            row_count,
            selected_count);
        return 0;
    }

    char csv_path[MAX_PATH] = {0};
    int wrote = kbo_captain_write_selection_csv(
        rows,
        row_count,
        source != NULL ? source : "captain_selection",
        csv_path,
        sizeof(csv_path));
    if (wrote) {
        append_logf(
            "KBO captain selection written source=%s date=%u season=%u league_id=%u rows=%d selected=%d csv=%s",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            row_count,
            selected_count,
            csv_path);
        kbo_emit_captain_initial_selection_news(
            date,
            season,
            league_id,
            rows,
            row_count,
            source != NULL ? source : "captain_selection");
    }
    return wrote;
}

static int kbo_captain_write_missing_selection_csv(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint8_t phase,
    const char* source)
{
    if (kbo_captain_selection_csv_exists(season)) {
        return 0;
    }

    LONG last_attempt = InterlockedCompareExchange(&g_kbo_captain_last_attempted_season, 0, 0);
    if (last_attempt == (LONG)season) {
        return 0;
    }
    InterlockedExchange(&g_kbo_captain_last_attempted_season, (LONG)season);

    append_logf(
        "KBO captain selection bootstrap source=%s date=%u season=%u league_id=%u phase=%u reason=missing_csv",
        source != NULL ? source : "",
        date,
        season,
        league_id,
        (unsigned)phase);
    int wrote = kbo_captain_write_initial_selection(
        date,
        season,
        league_id,
        source != NULL ? source : "captain_missing_csv_bootstrap");
    InterlockedExchange(&g_kbo_captain_last_attempted_season, 0);
    return wrote;
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

    uint32_t league_season = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    uint32_t season = kbo_captain_effective_season(date, league_season);
    int calendar_recovery = kbo_captain_calendar_season_recovery_active(date, league_season, phase);
    int calendar_preseason = kbo_captain_calendar_preseason_window_active(date, league_season, phase);
    if (season < 1982u || season > 2200u || (phase != 2u && !calendar_recovery && !calendar_preseason)) {
        return 0;
    }

    return kbo_captain_write_missing_selection_csv(
        date,
        season,
        league_id,
        phase,
        source != NULL
            ? source
            : (calendar_recovery
                ? "captain_calendar_year_recovery"
                : (calendar_preseason ? "captain_calendar_preseason_window" : "captain_preseason_phase2")));
}

static int kbo_run_captain_inseason_repair_once(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const char* source)
{
    KboCaptainSelectionRow current_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(current_rows, 0, sizeof(current_rows));
    int current_count = kbo_captain_load_selection_csv(season, current_rows, KBO_CAPTAIN_MAX_TEAMS);
    if (current_count <= 0) {
        return kbo_captain_write_initial_selection(
            date,
            season,
            league_id,
            source != NULL ? source : "captain_inseason_bootstrap");
    }

    uint32_t team_ids[KBO_CAPTAIN_MAX_TEAMS] = {0};
    int team_count = 0;
    int missing_count = 0;
    int departed_count = 0;
    if (!kbo_captain_current_rows_need_inseason_repair(
            league_id,
            current_rows,
            current_count,
            team_ids,
            &team_count,
            &missing_count,
            &departed_count)) {
        return 0;
    }

    KboCaptainSelectionRow candidate_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(candidate_rows, 0, sizeof(candidate_rows));
    int selected_count = 0;
    int candidate_count = kbo_captain_select_for_preseason(
        date,
        season,
        league_id,
        candidate_rows,
        KBO_CAPTAIN_MAX_TEAMS,
        &selected_count);
    if (candidate_count <= 0) {
        append_logf(
            "KBO captain in-season repair skipped source=%s date=%u season=%u league_id=%u reason=no_candidates missing=%d departed=%d",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            missing_count,
            departed_count);
        return 0;
    }

    KboCaptainSelectionRow merged_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(merged_rows, 0, sizeof(merged_rows));
    KboCaptainSelectionRow news_old_rows[KBO_CAPTAIN_MAX_TEAMS];
    KboCaptainSelectionRow news_new_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(news_old_rows, 0, sizeof(news_old_rows));
    memset(news_new_rows, 0, sizeof(news_new_rows));
    int merged_count = 0;
    int repaired_count = 0;
    int still_missing_count = 0;
    int news_count = 0;
    for (int i = 0; i < team_count && merged_count < KBO_CAPTAIN_MAX_TEAMS; i++) {
        uint32_t team_id = team_ids[i];
        int current_index = kbo_captain_find_row_index_by_team(current_rows, current_count, team_id);
        int candidate_index = kbo_captain_find_row_index_by_team(candidate_rows, candidate_count, team_id);
        int current_valid = current_index >= 0
            && kbo_captain_existing_row_still_with_team(&current_rows[current_index]);

        if (current_valid) {
            merged_rows[merged_count++] = current_rows[current_index];
            continue;
        }

        if (candidate_index >= 0) {
            merged_rows[merged_count] = candidate_rows[candidate_index];
            if (merged_rows[merged_count].player_id != 0u) {
                if (current_index >= 0 && current_rows[current_index].player_id != 0u) {
                    snprintf(
                        merged_rows[merged_count].reason,
                        sizeof(merged_rows[merged_count].reason),
                        "inseason_replacement:departed:%u",
                        current_rows[current_index].player_id);
                } else {
                    snprintf(
                        merged_rows[merged_count].reason,
                        sizeof(merged_rows[merged_count].reason),
                        "inseason_replacement:missing_row");
                }
                repaired_count++;
                if (news_count < KBO_CAPTAIN_MAX_TEAMS) {
                    if (current_index >= 0) {
                        news_old_rows[news_count] = current_rows[current_index];
                    }
                    news_new_rows[news_count] = merged_rows[merged_count];
                    news_count++;
                }
            } else {
                still_missing_count++;
            }
            merged_count++;
        } else if (current_index >= 0) {
            merged_rows[merged_count++] = current_rows[current_index];
            still_missing_count++;
        }
    }

    if (repaired_count <= 0) {
        if (still_missing_count > 0) {
            static uint32_t last_unresolved_date = 0u;
            static uint32_t last_unresolved_season = 0u;
            static uint32_t last_unresolved_league_id = 0u;
            static int last_unresolved_missing = -1;
            static int last_unresolved_departed = -1;
            static int last_unresolved_count = -1;
            if (date != last_unresolved_date
                    || season != last_unresolved_season
                    || league_id != last_unresolved_league_id
                    || missing_count != last_unresolved_missing
                    || departed_count != last_unresolved_departed
                    || still_missing_count != last_unresolved_count) {
                append_logf(
                    "KBO captain in-season repair skipped source=%s date=%u season=%u league_id=%u reason=unresolved_no_changes missing=%d departed=%d unresolved=%d",
                    source != NULL ? source : "",
                    date,
                    season,
                    league_id,
                    missing_count,
                    departed_count,
                    still_missing_count);
                last_unresolved_date = date;
                last_unresolved_season = season;
                last_unresolved_league_id = league_id;
                last_unresolved_missing = missing_count;
                last_unresolved_departed = departed_count;
                last_unresolved_count = still_missing_count;
            }
        }
        return 0;
    }

    char csv_path[MAX_PATH] = {0};
    int wrote = kbo_captain_write_selection_csv(
        merged_rows,
        merged_count,
        source != NULL ? source : "captain_inseason_repair",
        csv_path,
        sizeof(csv_path));
    if (wrote) {
        append_logf(
            "KBO captain in-season repair written source=%s date=%u season=%u league_id=%u repaired=%d missing=%d departed=%d unresolved=%d csv=%s",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            repaired_count,
            missing_count,
            departed_count,
            still_missing_count,
            csv_path);
        for (int i = 0; i < news_count; i++) {
            kbo_emit_captain_replacement_news(
                date,
                season,
                league_id,
                news_old_rows[i].player_id != 0u ? &news_old_rows[i] : NULL,
                &news_new_rows[i],
                source != NULL ? source : "captain_inseason_repair");
        }
    }
    return wrote;
}

static int kbo_run_captain_selection_maintenance_once(const char* source)
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

    uint32_t league_season = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    uint32_t season = kbo_captain_effective_season(date, league_season);
    if (season < 1982u || season > 2200u) {
        return 0;
    }

    int calendar_recovery = kbo_captain_calendar_season_recovery_active(date, league_season, phase);
    int calendar_preseason = kbo_captain_calendar_preseason_window_active(date, league_season, phase);
    int csv_exists = kbo_captain_selection_csv_exists(season);
    kbo_captain_log_phase_observed(
        source,
        date,
        league_id,
        league_ptr,
        league_season,
        season,
        phase,
        csv_exists,
        calendar_recovery,
        calendar_preseason);

    static uint32_t last_thread_date = 0u;
    static uint32_t last_thread_league_id = 0u;
    static uint32_t last_thread_league_season = 0u;
    static uint32_t last_thread_effective_season = 0u;
    static uint8_t last_thread_phase = 0xffu;
    static int last_thread_csv_exists = -1;
    static int last_thread_calendar_recovery = -1;
    static int last_thread_calendar_preseason = -1;
    if (source != NULL
            && strcmp(source, "captain_selection_thread") == 0
            && date == last_thread_date
            && league_id == last_thread_league_id
            && league_season == last_thread_league_season
            && season == last_thread_effective_season
            && phase == last_thread_phase
            && csv_exists == last_thread_csv_exists
            && calendar_recovery == last_thread_calendar_recovery
            && calendar_preseason == last_thread_calendar_preseason) {
        return 0;
    }
    if (source != NULL && strcmp(source, "captain_selection_thread") == 0) {
        last_thread_date = date;
        last_thread_league_id = league_id;
        last_thread_league_season = league_season;
        last_thread_effective_season = season;
        last_thread_phase = phase;
        last_thread_csv_exists = csv_exists;
        last_thread_calendar_recovery = calendar_recovery;
        last_thread_calendar_preseason = calendar_preseason;
    }

    if (csv_exists) {
        KboCaptainSelectionRow summary_rows[KBO_CAPTAIN_MAX_TEAMS];
        memset(summary_rows, 0, sizeof(summary_rows));
        int summary_count = kbo_captain_load_selection_csv(season, summary_rows, KBO_CAPTAIN_MAX_TEAMS);
        if (summary_count > 0) {
            kbo_emit_captain_initial_selection_news(
                date,
                season,
                league_id,
                summary_rows,
                summary_count,
                source != NULL ? source : "captain_summary_maintenance");
        }
    }

    if (phase == 2u && !csv_exists) {
        return kbo_captain_write_missing_selection_csv(
            date,
            season,
            league_id,
            phase,
            source != NULL ? source : "captain_preseason_start");
    }
    if (calendar_preseason && !csv_exists) {
        return kbo_captain_write_missing_selection_csv(
            date,
            season,
            league_id,
            phase,
            source != NULL ? source : "captain_calendar_preseason_window");
    }
    if (phase == 3u) {
        if (!csv_exists) {
            return kbo_captain_write_missing_selection_csv(
                date,
                season,
                league_id,
                phase,
                source != NULL ? source : "captain_missed_preseason_recovery");
        }
        return kbo_run_captain_inseason_repair_once(
            date,
            season,
            league_id,
            source != NULL ? source : "captain_inseason_thread");
    }
    if (calendar_recovery) {
        if (!csv_exists) {
            return kbo_captain_write_missing_selection_csv(
                date,
                season,
                league_id,
                phase,
                source != NULL ? source : "captain_calendar_year_recovery");
        }
        return kbo_run_captain_inseason_repair_once(
            date,
            season,
            league_id,
            source != NULL ? source : "captain_calendar_year_repair");
    }
    return 0;
}

DWORD WINAPI kbo_captain_preseason_selection_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO captain selection maintenance thread started");

    while (kbo_runtime_threads_should_continue()) {
        kbo_run_captain_selection_maintenance_once("captain_selection_thread");
        if (!kbo_runtime_sleep_should_continue(5000)) {
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

    HANDLE thread = CreateThread(NULL, 0, kbo_captain_preseason_selection_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "captain selection maintenance");
    } else {
        InterlockedExchange(&g_kbo_captain_preseason_thread_started, 0);
        append_logf("KBO captain selection maintenance thread failed error=%lu", GetLastError());
    }
}
