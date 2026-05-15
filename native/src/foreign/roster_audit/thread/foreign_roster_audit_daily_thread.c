#include "../internal/foreign_roster_audit_internal.h"
#include "../../../custom_events/runtime/monitor/custom_event_monitor.h"
#include "../../../team/add_player_guard/team_add_player_guard_ai_roster.h"
#include "../../../team/independent_acquisition/independent_acquisition_ai.h"
#include "../../injury/api/foreign_injury.h"
#include "../../retention_guard/foreign_retention_guard.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../../../fa_declaration/fa_declaration.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"

static int kbo_foreign_roster_daily_abort_if_save(const char* stage, uint32_t today)
{
    if (!kbo_runtime_save_in_progress()) {
        return 0;
    }

    kbo_log_runtimef(
        "foreign roster daily audit deferred reason=save_in_progress stage=%s today=%u",
        stage != NULL ? stage : "",
        today);
    return 1;
}

DWORD WINAPI kbo_foreign_roster_daily_audit_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("foreign roster daily audit thread started");

    uint32_t last_audit_date = 0u;
    uint32_t last_custom_event_scheduled_date = 0u;
    uint32_t last_custom_event_scanned_date = 0u;
    uint32_t last_custom_event_fa_comp_date = 0u;
    uint32_t observed_date = 0u;
    int stable_date_ticks = 0;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->foreign_roster_daily_audit_sleep_ms)) {
            break;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_roster_daily_audit")) {
            break;
        }

        uint32_t today = 0u;
        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_yyyymmdd(&today)
                || !kbo_get_current_save_path(save_path, sizeof(save_path))) {
            continue;
        }

        if (today == 0u || today == last_audit_date) {
            continue;
        }
        if (today != observed_date) {
            observed_date = today;
            stable_date_ticks = 1;
            continue;
        }
        stable_date_ticks++;
        if (stable_date_ticks < 2) {
            continue;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_roster_daily_date_change")) {
            break;
        }

        if (kbo_foreign_roster_daily_abort_if_save("before_rights_sync", today)) {
            continue;
        }
        kbo_sync_active_foreign_waiver_rights_to_memory(
            "foreign_roster_daily_date_change",
            today);
        if (kbo_foreign_roster_daily_abort_if_save("after_rights_sync", today)) {
            continue;
        }
        kbo_custom_event_monitor_tick(
            &last_custom_event_scheduled_date,
            &last_custom_event_scanned_date,
            &last_custom_event_fa_comp_date,
            "foreign_roster_daily_date_change");
        if (kbo_foreign_roster_daily_abort_if_save("after_custom_events", today)) {
            continue;
        }
        kbo_run_foreign_ai_roster_daily_callup("foreign_roster_daily_date_change");
        if (kbo_foreign_roster_daily_abort_if_save("after_ai_roster_callup", today)) {
            continue;
        }
        kbo_run_independent_team_acquisition_ai("foreign_roster_daily_date_change");
        if (kbo_foreign_roster_daily_abort_if_save("after_independent_acquisition", today)) {
            continue;
        }
        kbo_foreign_injury_replacement_scan_once("foreign_roster_daily_date_change");
        if (kbo_foreign_roster_daily_abort_if_save("after_injury_replacement", today)) {
            continue;
        }
        kbo_foreign_retention_guard_repair("foreign_roster_daily_date_change");
        if (kbo_foreign_roster_daily_abort_if_save("after_retention_guard", today)) {
            continue;
        }
        uint32_t season = today / 10000u;
        kbo_fa_declaration_repair_retained_contracts_for_season(
            season,
            "foreign_roster_daily_date_change");
        if (kbo_foreign_roster_daily_abort_if_save("after_fa_repair_current", today)) {
            continue;
        }
        if (season > 1982u) {
            kbo_fa_declaration_repair_retained_contracts_for_season(
                season - 1u,
                "foreign_roster_daily_date_change_previous_season");
        }
        if (kbo_foreign_roster_daily_abort_if_save("after_fa_repair_previous", today)) {
            continue;
        }
        audit_foreign_roster_state("foreign_roster_daily_date_change", 1);
        last_audit_date = today;
    }

    InterlockedExchange(&g_kbo_foreign_roster_daily_audit_started, 0);
    kbo_log_runtime_line("foreign roster daily audit thread stopped");
    return 0;
}

void start_kbo_foreign_roster_daily_audit_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_foreign_roster_daily_audit_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(kbo_foreign_roster_daily_audit_thread, NULL, "foreign roster daily audit")) {
        InterlockedExchange(&g_kbo_foreign_roster_daily_audit_started, 0);
    }
}
