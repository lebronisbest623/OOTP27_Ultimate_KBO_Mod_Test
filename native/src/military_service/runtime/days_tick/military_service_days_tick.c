#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../amateur_player_quality/api/amateur_player_quality.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/news/history_stubs/core_history_stubs.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/season/opening_day_storyline_guard.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../../fa_market_classification/api/fa_market_classification.h"
#include "../../../fa_requalification/fa_requalification.h"
#include "../../../foreign/replacement_seed/api/foreign_replacement_seed.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../military_service.h"
#include "../../players/loans/military_active_loan.h"
#include "../../players/state/military_player_state.h"
#include "../../returns/military_return.h"
#include "../../returns/military_return_preview_news.h"
#include "../../calendar/military_service_date.h"
#include "../../seed/registry/military_seed_registry.h"
#include "../assignment/military_service_assignment.h"
#include "../state/military_service_runtime_state.h"
#include "military_service_days_tick_internal.h"
#include "military_service_tick.h"

int kbo_tick_military_service_days(const char* source, int* out_seeded_assignments)
{
    if (out_seeded_assignments != NULL) {
        *out_seeded_assignments = 0;
    }
    if (!kbo_fix_enabled()) {
        return 0;
    }

    uint32_t today_serial = kbo_current_date_serial();
    if (today_serial == 0) {
        return 0;
    }

    if (kbo_opening_day_storyline_guard_active(source, NULL, NULL)) {
        return 0;
    }

    flush_pending_special_player_history_sql(source);

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
    int return_preview_news = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        uint32_t service_team_id = 0u;
        if (current_team_id == sang_id || current_team_id == kpb_id) {
            service_team_id = current_team_id;
        } else if (loan_team_id == sang_id || loan_team_id == kpb_id) {
            service_team_id = loan_team_id;
        }
        if (service_team_id == 0u) {
            continue;
        }

        tracked++;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int active_index = find_active_kbo_military_loan_index(player_id);
        int32_t direct_days_left = kbo_military_days_left(player);
        uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        if (active_index < 0 && military_active == 0u && direct_days_left <= 0) {
            if (!source_allows_roster_mutation) {
                deferred_invalid_releases++;
                continue;
            }
            uint8_t* service_team = (service_team_id == sang_id) ? sang : kpb;
            invalid_released += kbo_release_invalid_military_service_team_assignment(
                player,
                service_team,
                service_team_id,
                source,
                vector_offset);
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        kbo_military_resolve_original_team(
            player,
            service_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id);
        if (original_team_id != 0u
                && original_team_id != service_team_id
                && original_team_id != sang_id
                && original_team_id != kpb_id) {
            kbo_military_repair_original_team_memory(
                player,
                original_team_id,
                original_league_id,
                service_team_id,
                sang_id,
                kpb_id);
        }

        if (active_index < 0
                && direct_days_left > 0
                && original_team_id != 0
                && original_team_id != service_team_id
                && original_team_id != sang_id
                && original_team_id != kpb_id) {
            uint8_t* service_team   = (service_team_id == sang_id) ? sang : kpb;
            uint32_t service_league = service_team != NULL
                ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET)
                : 0;
            register_active_kbo_military_loan(
                player_id, player_ptr,
                original_team_id, original_league_id,
                service_team_id,
                service_league != 0 ? service_league
                    : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET));
            player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 1u;
            active_index = find_active_kbo_military_loan_index(player_id);
            newly_registered++;
            static volatile LONG discovered_register_log_count = 0;
            LONG discovered_slot = InterlockedIncrement(&discovered_register_log_count);
            if (discovered_slot <= 160) {
                kbo_log_runtimef(
                    "KBO military discovered active loan registered source=%s player=%u service_team=%u original_team=%u original_league=%u days_left=%d military_active=%u",
                    source != NULL ? source : "",
                    player_id,
                    service_team_id,
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
                uint8_t* service_team = (service_team_id == sang_id) ? sang : kpb;
                invalid_released += kbo_release_invalid_military_service_team_assignment(
                    player,
                    service_team,
                    service_team_id,
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

    return_preview_news = kbo_emit_military_return_preview_news_if_due(today_serial, source);

    LONG log_index = InterlockedIncrement(&g_military_days_tick_log_count);
    if (seeded_assignments > 0
            || returned > 0
            || newly_registered > 0
            || managed > 0
            || invalid_released > 0
            || return_preview_news > 0
            || deferred_returns > 0
            || deferred_invalid_releases > 0
            || log_index <= 20) {
        kbo_log_runtimef(
            "KBO military service day tick source=%s date_serial=%u"
            " seeded=%d tracked=%d newly_registered=%d monitored=%d managed=%d returned=%d invalid_released=%d"
            " return_preview_news=%d deferred_returns=%d deferred_invalid=%d count=%d vector_off=0x%x",
            source != NULL ? source : "",
            today_serial,
            seeded_assignments,
            tracked, newly_registered, monitored, managed, returned,
            invalid_released,
            return_preview_news,
            deferred_returns,
            deferred_invalid_releases,
            player_count, vector_offset);
    }

    return returned;
}

DWORD WINAPI kbo_military_days_tick_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO military service day tick thread started");
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->military_days_tick_sleep_ms)) {
            break;
        }
        kbo_tick_military_service_days("military_days_tick", NULL);
    }
    InterlockedExchange(&g_military_days_tick_started, 0);
    kbo_log_runtime_line("KBO military service day tick thread stopped");
    return 0;
}

