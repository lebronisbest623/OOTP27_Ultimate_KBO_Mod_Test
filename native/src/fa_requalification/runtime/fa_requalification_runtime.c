#include "../fa_requalification_internal.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"

int kbo_restore_fa_requalification_team_control(
    const KboFaRequalificationRecord* rec,
    uint32_t current_year,
    const char* source)
{
    if (rec == NULL || rec->player_id == 0u || rec->original_team_id == 0u
            || rec->last_fa_year < 1982u) {
        return 0;
    }
    uint32_t team_control_years = kbo_fa_requalification_team_control_years();
    if (current_year >= rec->last_fa_year + team_control_years) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=already_eligible current_year=%u last_fa_year=%u eligible_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year,
                rec->last_fa_year + team_control_years);
        }
        return 0;
    }

    uint8_t* player = kbo_find_fa_requalification_player_by_id(rec->player_id);
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->original_team_id, 1);
    if (player == NULL) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=player_not_found current_year=%u last_fa_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year);
        }
        return 0;
    }
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification skipped source=%s player=%u team=%u reason=team_not_found current_year=%u last_fa_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year);
        }
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t old_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t old_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t old_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int changed = 0;

    if (old_current_team != rec->original_team_id) {
        if (old_current_team != 0u) {
            uint8_t* old_team = find_kbo_team_by_numeric_id_any_league(old_current_team, 1);
            if (old_team != NULL) {
                kbo_remove_player_id_from_known_team_roster_arrays(old_team, rec->player_id);
            }
        }
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = rec->original_team_id;
        changed = 1;
    }
    if (old_active_team != rec->original_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = rec->original_team_id;
        changed = 1;
    }
    if (league_id != 0u && old_league != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (league_id != 0u && *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    kbo_add_player_id_to_team_assignment_arrays(team, rec->player_id);

    if (changed) {
        append_logf(
            "KBO FA requalification restored team control source=%s player=%u team=%u year=%u last_fa_year=%u eligible_year=%u old_team=%u old_active=%u old_league=%u league=%u",
            source != NULL ? source : "",
            rec->player_id,
            rec->original_team_id,
            current_year,
            rec->last_fa_year,
            rec->last_fa_year + team_control_years,
            old_current_team,
            old_active_team,
            old_league,
            league_id);
    } else {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_skip_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO FA requalification checked source=%s player=%u team=%u reason=already_controlled current_year=%u last_fa_year=%u eligible_year=%u",
                source != NULL ? source : "",
                rec->player_id,
                rec->original_team_id,
                current_year,
                rec->last_fa_year,
                rec->last_fa_year + team_control_years);
        }
    }
    return changed;
}

void kbo_run_fa_requalification_once(const char* source)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_no_date_log_count);
        if (slot <= 20 || (slot % 60) == 0) {
            append_logf(
                "KBO FA requalification skipped source=%s reason=current_date_unavailable count=%ld",
                source != NULL ? source : "",
                slot);
        }
        return;
    }
    uint32_t current_year = 0u;
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id != 0u) {
        current_year = kbo_find_league_year_from_id(kbo_league_id);
    }
    if (current_year < 1982u || current_year > 2200u) {
        current_year = today / 10000u;
    }
    if (current_year < 1982u || current_year > 2200u) {
        append_logf(
            "KBO FA requalification skipped source=%s reason=invalid_current_year today=%u current_year=%u",
            source != NULL ? source : "",
            today,
            current_year);
        return;
    }

    KboFaRequalificationRecord records[KBO_FA_REQUALIFICATION_MAX];
    kbo_lock_fa_requalification_records();
    int count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    kbo_unlock_fa_requalification_records();
    if (count <= 0) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_requalification_no_records_log_count);
        if (g_kbo_fa_requalification_last_no_records_date != today || slot <= 5 || (slot % 60) == 0) {
            char path[MAX_PATH] = {0};
            get_kbo_fa_requalification_path(path, sizeof(path));
            append_logf(
                "KBO FA requalification pass source=%s records=0 restored=0 today=%u path=%s",
                source != NULL ? source : "",
                today,
                path);
            g_kbo_fa_requalification_last_no_records_date = today;
        }
        return;
    }
    int restored = 0;
    for (int i = 0; i < count; i++) {
        restored += kbo_restore_fa_requalification_team_control(&records[i], current_year, source);
    }
    append_logf("KBO FA requalification pass source=%s records=%d restored=%d today=%u", source != NULL ? source : "", count, restored, today);
}

DWORD WINAPI kbo_fa_requalification_thread(LPVOID parameter)
{
    (void)parameter;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(kbo_runtime_tuning_policy()->fa_requalification_thread_sleep_ms)) {
            break;
        }
        kbo_run_fa_requalification_once("fa_requalification_monitor");
    }
    InterlockedExchange(&g_kbo_fa_requalification_thread_started, 0);
    append_log_line("KBO FA requalification monitor thread stopped");
    return 0;
}

void start_kbo_fa_requalification_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_fa_requalification_thread_started, 1, 0) != 0) {
        return;
    }
    HANDLE thread = CreateThread(NULL, 0, kbo_fa_requalification_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "FA requalification monitor");
        append_log_line("KBO FA requalification monitor thread started");
    } else {
        InterlockedExchange(&g_kbo_fa_requalification_thread_started, 0);
        append_log_line("KBO FA requalification monitor thread failed to start");
    }
}

