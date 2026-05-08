#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../amateur_player_quality/amateur_player_quality.h"
#include "../bootstrap/ootp_offsets.h"
#include "../core/core_current_date.h"
#include "../core/core_flags/flags_api.h"
#include "../core/core_history_stubs.h"
#include "../core/core_league_context_parts/league_context_lookup.h"
#include "../core/core_log.h"
#include "../core/core_save_paths.h"
#include "../team/team_assignment.h"
#include "../team/team_lookup.h"
#include "../team/team_roster_arrays.h"
#include "military_active_loan.h"
#include "military_player_state.h"
#include "military_return.h"
#include "military_seed_registry.h"
#include "military_service_date.h"
#include "military_service_seed_parse.h"
#include "military_service_team_policy.h"
#include "military_service_tick.h"

/* Military service daily tick state. */

static LONG g_military_days_tick_started = 0;
static LONG g_military_days_tick_log_count = 0;
static LONG g_military_daily_mutation_ready_log_count = 0;
static LONG g_military_seed_bootstrap_started = 0;
static LONG g_military_seed_expired_skip_log_count = 0;

static int kbo_military_daily_roster_mutation_window_ready(
    uint32_t today_serial,
    int32_t player_count)
{
    char save_path[MAX_PATH] = {0};
    if (today_serial == 0u
            || player_count <= 0
            || !kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    LONG slot = InterlockedIncrement(&g_military_daily_mutation_ready_log_count);
    if (slot <= 20 || (slot % 100) == 0) {
        append_logf(
            "KBO military daily roster mutation window immediate date_serial=%u player_count=%d save=%s slot=%ld",
            today_serial,
            player_count,
            save_path,
            slot);
    }
    return 1;
}

/* Applying configured military service seed assignments to players. Included from native/src/military_service_loan.inc. */

static int kbo_apply_military_service_seed_assignments(uint8_t* sang, uint8_t* kpb, const char* source)
{
    if (sang == NULL && kpb == NULL) {
        return 0;
    }

    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t today_serial = kbo_current_date_serial();
    int applied = 0;
    KboMilitaryServiceSeed seeds[KBO_MILITARY_SERVICE_SEED_MAX];
    int seed_count = kbo_snapshot_military_service_seeds(seeds, KBO_MILITARY_SERVICE_SEED_MAX);

    for (int i = 0; i < seed_count; i++) {
        KboMilitaryServiceSeed* seed = &seeds[i];
        if (seed->player_id == 0u) {
            continue;
        }

        uint8_t* player = kbo_military_find_player_by_id(seed->player_id);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            continue;
        }

        uint8_t* service_team = (seed->service_team_code[0] != '\0' && _stricmp(seed->service_team_code, "KPB") == 0)
            ? kpb
            : sang;
        uint32_t service_team_id = service_team != NULL
            ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_ID_OFFSET)
            : 0u;
        if (service_team_id == 0u) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        uint32_t seed_return_serial = kbo_military_yyyymmdd_to_serial(seed->service_return_yyyymmdd);
        if (today_serial != 0u
                && seed_return_serial != 0u
                && seed_return_serial <= today_serial
                && current_team_id != service_team_id
                && loan_team_id != service_team_id) {
            LONG skip_log = InterlockedIncrement(&g_military_seed_expired_skip_log_count);
            if (skip_log <= 40) {
                append_logf(
                    "KBO military service seed skipped expired key=%s player=%u return=%u today_serial=%u current_team=%u service_team=%u",
                    seed->key,
                    seed->player_id,
                    seed->service_return_yyyymmdd,
                    today_serial,
                    current_team_id,
                    service_team_id);
            }
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        if (seed->original_team_code[0] != '\0') {
            uint8_t* original_team = NULL;
            if (seed->original_team_code[0] >= '0' && seed->original_team_code[0] <= '9') {
                original_team = find_kbo_team_by_numeric_id_any_league((uint32_t)strtoul(seed->original_team_code, NULL, 10), 0);
            } else {
                original_team = find_kbo_team_by_csv_id_any_league(seed->original_team_code, 0);
            }
            if (original_team != NULL) {
                original_team_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_ID_OFFSET);
                original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
        }
        if (original_team_id == 0u) {
            original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
        }
        if (original_team_id == 0u
                || original_team_id == service_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        }
        if (original_team_id == 0u
                || original_team_id == service_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            continue;
        }
        if (original_league_id == 0u) {
            uint8_t* original_team = find_kbo_team_by_numeric_id_any_league(original_team_id, 0);
            if (original_team != NULL) {
                original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
        }

        if (current_team_id != service_team_id && loan_team_id != service_team_id) {
            int called_pre_change = 0;
            int called_register = 0;
            int called_attach = 0;
            uint32_t service_league_id = *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            kbo_assign_player_to_team_internal(
                player,
                service_team,
                service_league_id,
                0,
                &called_pre_change,
                &called_register,
                &called_attach);
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_team_id;
            *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = original_team_id;
            if (original_league_id != 0u) {
                *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = original_league_id;
            }
            append_logf(
                "KBO military service seed assigned to service key=%s player=%u old_team=%u service_team=%u original_team=%u pre=%d register=%d attach=%d",
                seed->key,
                seed->player_id,
                current_team_id,
                service_team_id,
                original_team_id,
                called_pre_change,
                called_register,
                called_attach);
        }

        register_active_kbo_military_loan(
            seed->player_id,
            (uintptr_t)player,
            original_team_id,
            original_league_id,
            service_team_id,
            *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET));

        int active_index = find_active_kbo_military_loan_index(seed->player_id);
        if (active_index >= 0) {
            KboMilitaryActiveLoan* loan = kbo_active_military_loan_at(active_index);
            if (loan == NULL) {
                continue;
            }
            uint32_t start_serial = kbo_military_yyyymmdd_to_serial(seed->service_start_yyyymmdd);
            if (start_serial != 0u) {
                loan->service_start_date_serial = start_serial;
            }
            loan->service_total_days = seed->service_total_days > 0
                ? seed->service_total_days
                : KBO_MILITARY_SERVICE_DAYS;
            uint32_t return_serial = seed_return_serial;
            if (return_serial == 0u && loan->service_start_date_serial != 0u) {
                return_serial = loan->service_start_date_serial + (uint32_t)loan->service_total_days;
            }
            if (return_serial != 0u) {
                loan->service_return_date_serial = return_serial;
            }
            if (today_serial != 0u && loan->service_return_date_serial != 0u) {
                kbo_set_military_days_left(
                    player,
                    kbo_military_days_left_from_return_serial(loan->service_return_date_serial, today_serial));
            }
            player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 1;
            kbo_clear_military_unavailable_flags(player);
            applied++;
        }
    }

    if (applied > 0) {
        append_logf("KBO military service seed applied source=%s assignments=%d", source != NULL ? source : "", applied);
    }
    return applied;
}


static int kbo_release_invalid_military_service_team_assignment(
    uint8_t* player,
    uint8_t* service_team,
    uint32_t service_team_id,
    const char* source,
    uint32_t vector_offset)
{
    if (player == NULL
            || service_team == NULL
            || service_team_id == 0u
            || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return 0;
    }

    uint32_t old_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t old_current_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t old_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t old_original_slot_team = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint8_t old_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    int32_t old_contract_status = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
    int32_t old_salary_y1 = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t old_military_days_left = kbo_military_days_left(player);
    uint8_t old_military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
    int removed = kbo_remove_player_id_from_known_team_roster_arrays(service_team, player_id);

    uint32_t fallback_league_id = kbo_resolve_kbo_league_id();
    if (fallback_league_id == 0u) {
        fallback_league_id = old_current_league;
    }

    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = fallback_league_id;
    if (old_active_team == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (old_original_slot_team == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0u;
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0u;
        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0;
        player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] = 1;
    }

    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET) = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 0u;
    memset(
        player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
        0,
        OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t));
    kbo_clear_military_status_flags(player);
    kbo_set_military_days_left(player, 0);

    static volatile LONG invalid_release_log_count = 0;
    LONG slot = InterlockedIncrement(&invalid_release_log_count);
    if (slot <= 160) {
        append_logf(
            "KBO military invalid FA assignment released source=%s player=%u service_team=%u old_current_team=%u old_current_league=%u new_league=%u old_active_team=%u old_original_slot=%u removed=%d old_contract_level=%u old_contract_status=%d old_salary_y1=%d old_days_left=%d old_military_active=%u vector_off=0x%x",
            source != NULL ? source : "",
            player_id,
            service_team_id,
            old_current_team,
            old_current_league,
            fallback_league_id,
            old_active_team,
            old_original_slot_team,
            removed,
            (unsigned)old_contract_level,
            old_contract_status,
            old_salary_y1,
            old_military_days_left,
            (unsigned)old_military_active,
            vector_offset);
    }
    return 1;
}

int kbo_tick_military_service_days(const char* source, int* out_seeded_assignments)
{
    if (out_seeded_assignments != NULL) {
        *out_seeded_assignments = 0;
    }
    if (!kbo_fix_enabled()) {
        return 0;
    }

    flush_pending_special_player_history_sql(source);

    uint32_t today_serial = kbo_current_date_serial();
    if (today_serial == 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t   player_count  = 0;
    uint32_t  vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        return 0;
    }

    kbo_update_amateur_reputation_from_team_records(source);

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    if (sang_id == 0 && kpb_id == 0) {
        return 0;
    }
    int seeded_assignments = 0;
    int source_is_daily_tick = source != NULL && strcmp(source, "military_days_tick") == 0;
    int source_allows_seed_assignment = !source_is_daily_tick;
    int source_allows_roster_mutation = !source_is_daily_tick
        || kbo_military_daily_roster_mutation_window_ready(today_serial, player_count);
    if (source_allows_seed_assignment) {
        seeded_assignments = kbo_apply_military_service_seed_assignments(sang, kpb, source);
    }
    if (out_seeded_assignments != NULL) {
        *out_seeded_assignments = seeded_assignments;
    }

    int tracked          = 0;
    int monitored        = 0;
    int managed          = 0;
    int returned         = 0;
    int newly_registered = 0;
    int invalid_released = 0;
    int deferred_returns = 0;
    int deferred_invalid_releases = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (current_team_id == 0
                || (current_team_id != sang_id && current_team_id != kpb_id)) {
            continue;
        }

        tracked++;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int active_index = find_active_kbo_military_loan_index(player_id);
        int32_t direct_days_left = kbo_military_days_left(player);
        uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        if (active_index < 0 && military_active == 0u) {
            if (!source_allows_roster_mutation) {
                deferred_invalid_releases++;
                continue;
            }
            uint8_t* service_team = (current_team_id == sang_id) ? sang : kpb;
            invalid_released += kbo_release_invalid_military_service_team_assignment(
                player,
                service_team,
                current_team_id,
                source,
                vector_offset);
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        kbo_military_resolve_original_team(
            player,
            current_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id);
        if (original_team_id != 0u
                && original_team_id != current_team_id
                && original_team_id != sang_id
                && original_team_id != kpb_id) {
            kbo_military_repair_original_team_memory(
                player,
                original_team_id,
                original_league_id,
                current_team_id,
                sang_id,
                kpb_id);
        }

        if (active_index < 0
                && direct_days_left > 0
                && original_team_id != 0
                && original_team_id != current_team_id
                && original_team_id != sang_id
                && original_team_id != kpb_id) {
            uint8_t* service_team   = (current_team_id == sang_id) ? sang : kpb;
            uint32_t service_league = service_team != NULL
                ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET)
                : 0;
            register_active_kbo_military_loan(
                player_id, player_ptr,
                original_team_id, original_league_id,
                current_team_id,
                service_league != 0 ? service_league
                    : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET));
            active_index = find_active_kbo_military_loan_index(player_id);
            newly_registered++;
            static volatile LONG discovered_register_log_count = 0;
            LONG discovered_slot = InterlockedIncrement(&discovered_register_log_count);
            if (discovered_slot <= 160) {
                append_logf(
                    "KBO military discovered active loan registered source=%s player=%u service_team=%u original_team=%u original_league=%u days_left=%d military_active=%u",
                    source != NULL ? source : "",
                    player_id,
                    current_team_id,
                    original_team_id,
                    original_league_id,
                    direct_days_left,
                    (unsigned)military_active);
            }
        }

        if (active_index < 0) {
            int32_t days_left = kbo_military_days_left(player);
            if (original_team_id == 0u
                    && days_left <= 0
                    && player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0) {
                if (!source_allows_roster_mutation) {
                    deferred_invalid_releases++;
                    continue;
                }
                uint8_t* service_team = (current_team_id == sang_id) ? sang : kpb;
                invalid_released += kbo_release_invalid_military_service_team_assignment(
                    player,
                    service_team,
                    current_team_id,
                    source,
                    vector_offset);
                continue;
            }
            if (days_left <= 0) {
                if (!source_allows_roster_mutation) {
                    deferred_returns++;
                    continue;
                }
                returned += kbo_return_completed_military_loan_player(player, source, vector_offset, 0);
                continue;
            }
            kbo_clear_military_unavailable_flags(player);
            continue;
        }

        KboMilitaryActiveLoan* loan = kbo_active_military_loan_at(active_index);
        if (loan == NULL) {
            continue;
        }
        loan->player_ptr = player_ptr;
        if (loan->service_total_days <= 0) {
            loan->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        }
        if (loan->service_start_date_serial == 0
                || loan->service_start_date_serial > today_serial) {
            loan->service_start_date_serial = today_serial;
        }
        if (loan->service_return_date_serial == 0u) {
            int32_t stored_days_left = kbo_military_days_left(player);
            loan->service_return_date_serial = today_serial + (uint32_t)(
                stored_days_left > 0 ? stored_days_left : loan->service_total_days);
        }

        int32_t days_left = kbo_military_effective_days_left(player);
        int32_t stored_days_left = kbo_military_days_left(player);
        int32_t managed_days_left = kbo_military_days_left_from_return_serial(
            loan->service_return_date_serial,
            today_serial);
        if (managed_days_left != days_left) {
            days_left = managed_days_left;
        }
        if (stored_days_left != managed_days_left) {
            kbo_set_military_days_left(player, managed_days_left);
            managed++;
        }
        if (days_left <= 0) {
            if (!source_allows_roster_mutation) {
                deferred_returns++;
                continue;
            }
            returned += kbo_return_completed_military_loan_player(player, source, vector_offset, 0);
            continue;
        }

        kbo_clear_military_unavailable_flags(player);
        monitored++;
    }

    LONG log_index = InterlockedIncrement(&g_military_days_tick_log_count);
    if (seeded_assignments > 0
            || returned > 0
            || newly_registered > 0
            || managed > 0
            || invalid_released > 0
            || deferred_returns > 0
            || deferred_invalid_releases > 0
            || log_index <= 20) {
        append_logf(
            "KBO military service day tick source=%s date_serial=%u"
            " seeded=%d tracked=%d newly_registered=%d monitored=%d managed=%d returned=%d invalid_released=%d"
            " deferred_returns=%d deferred_invalid=%d count=%d vector_off=0x%x",
            source != NULL ? source : "",
            today_serial,
            seeded_assignments,
            tracked, newly_registered, monitored, managed, returned,
            invalid_released,
            deferred_returns,
            deferred_invalid_releases,
            player_count, vector_offset);
    }

    return returned;
}

static DWORD WINAPI kbo_military_days_tick_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO military service day tick thread started");
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(5000)) {
            break;
        }
        kbo_tick_military_service_days("military_days_tick", NULL);
    }
    InterlockedExchange(&g_military_days_tick_started, 0);
    append_log_line("KBO military service day tick thread stopped");
    return 0;
}

static DWORD WINAPI kbo_military_seed_bootstrap_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO military service seed bootstrap thread started");

    char last_save_path[MAX_PATH] = {0};
    int settled_attempts = 0;
    for (int attempt = 1; attempt <= 36; attempt++) {
        if (!kbo_runtime_sleep_should_continue(attempt == 1 ? 2500u : 5000u)) {
            break;
        }

        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
            if (attempt <= 8 || attempt % 6 == 0) {
                append_logf("KBO military service seed bootstrap waiting attempt=%d reason=no_save_path", attempt);
            }
            continue;
        }

        if (_stricmp(last_save_path, save_path) != 0) {
            snprintf(last_save_path, sizeof(last_save_path), "%s", save_path);
            settled_attempts = 0;
        }

        uint32_t today_serial = kbo_current_date_serial();
        uintptr_t player_vector = 0;
        int32_t player_count = 0;
        uint32_t vector_offset = 0;
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        uint8_t* kpb = find_kbo_team_by_csv_id_any_league("KPB", 0);
        if (today_serial == 0u
                || !find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)
                || (sang == NULL && kpb == NULL)) {
            if (attempt <= 8 || attempt % 6 == 0) {
                append_logf(
                    "KBO military service seed bootstrap waiting attempt=%d reason=state_not_ready date_serial=%u player_count=%d sang=%p kpb=%p save=%s",
                    attempt,
                    today_serial,
                    player_count,
                    (void*)sang,
                    (void*)kpb,
                    save_path);
            }
            continue;
        }

        int seeded = 0;
        int returned = kbo_tick_military_service_days("military_seed_bootstrap", &seeded);
        if (seeded > 0 || returned > 0) {
            append_logf(
                "KBO military service seed bootstrap applied attempt=%d seeded=%d returned=%d save=%s",
                attempt,
                seeded,
                returned,
                save_path);
            return 0;
        }

        if (GetFileAttributesA(save_path) != INVALID_FILE_ATTRIBUTES) {
            settled_attempts++;
        }
        if (settled_attempts >= 3) {
            append_logf(
                "KBO military service seed bootstrap settled attempt=%d seeded=0 returned=0 save=%s",
                attempt,
                save_path);
            return 0;
        }
    }

    append_log_line("KBO military service seed bootstrap ended without settled save");
    return 0;
}

void start_kbo_military_seed_bootstrap_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_military_seed_bootstrap_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_military_seed_bootstrap_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_military_seed_bootstrap_started, 0);
        append_log_line("KBO military service seed bootstrap thread failed to start");
    }
}

void start_kbo_military_days_tick_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_military_days_tick_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_military_days_tick_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_military_days_tick_started, 0);
        append_log_line("KBO military service day tick thread failed to start");
    }
}

