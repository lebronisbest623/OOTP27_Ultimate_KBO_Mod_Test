#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai.h"
#include "ai/independent_acquisition_ai_internal.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/logging/core_log.h"
#include "../../core/teams/core_team_collect.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/policy/foreign_player_policy.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/injury/api/foreign_injury_labels.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../classification/team_classification.h"
#include "../control/team_human_control.h"
#include "../lookup/team_lookup.h"
#include "independent_acquisition_window.h"

static volatile LONG g_kbo_independent_acquisition_ai_last_run_date = 0;

static int kbo_independent_acquisition_claim_daily_run(uint32_t today)
{
    if (today == 0u) {
        return 0;
    }

    LONG last = InterlockedCompareExchange(
        &g_kbo_independent_acquisition_ai_last_run_date,
        0,
        0);
    if ((uint32_t)last == today) {
        return 0;
    }

    return InterlockedCompareExchange(
        &g_kbo_independent_acquisition_ai_last_run_date,
        (LONG)today,
        last) == last;
}

static int kbo_independent_acquisition_window_active(uint32_t today)
{
    uint32_t open_date = kbo_independent_team_acquisition_window_open_date();
    if (today == 0u || open_date == 0u || today < open_date) {
        return 0;
    }

    uint32_t open_serial = kbo_date_serial(
        open_date / 10000u,
        (open_date / 100u) % 100u,
        open_date % 100u);
    uint32_t today_serial = kbo_date_serial(
        today / 10000u,
        (today / 100u) % 100u,
        today % 100u);
    if (open_serial == 0u || today_serial == 0u || today_serial < open_serial) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    uintptr_t league_ptr = league_id != 0u ? kbo_find_league_ptr_from_id(league_id) : 0u;
    if (league_ptr == 0u
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET),
                sizeof(uint8_t))) {
        return today_serial - open_serial <= (uint32_t)kbo_foreign_player_policy()->pending_offer_ttl_days;
    }

    uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    if (phase != 2u && phase != 3u) {
        static uint32_t last_logged_closed_date = 0u;
        if (last_logged_closed_date != today) {
            last_logged_closed_date = today;
            kbo_log_runtimef(
                "independent acquisition AI window closed source=league_phase today=%u open=%u league=%u phase=%u",
                today,
                open_date,
                league_id,
                (uint32_t)phase);
        }
        return 0;
    }

    return 1;
}

int kbo_run_independent_team_acquisition_ai(const char* source)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || read_kbo_localappdata_flag_file("disable_independent_acquisition_ai.txt")) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)
            || !kbo_independent_acquisition_window_active(today)) {
        return 0;
    }

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    int seed_rows = 0;
    int unresolved_rows = 0;
    int seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        &seed_rows,
        &unresolved_rows);
    if (seller_count <= 0) {
        kbo_log_runtimef(
            "independent acquisition AI skipped source=%s reason=no_resolved_seller today=%u seed_rows=%d unresolved=%d",
            source != NULL ? source : "",
            today,
            seed_rows,
            unresolved_rows);
        return 0;
    }

    uint32_t season = today / 10000u;
    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    KboIndependentFuturesTeamLeague available_sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(available_sellers, 0, sizeof(available_sellers));
    int available_seller_count = 0;
    int capped_sellers = 0;
    for (int i = 0; i < seller_count; i++) {
        int transfers = kbo_independent_acquisition_transferred_count(season, sellers[i].team_id);
        if (transfers >= seller_transfer_limit) {
            capped_sellers++;
            continue;
        }
        if (available_seller_count < KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS) {
            available_sellers[available_seller_count++] = sellers[i];
        }
    }
    if (capped_sellers > 0) {
        kbo_log_runtimef(
            "independent acquisition AI seller transfer limit source=%s today=%u limit=%d sellers=%d available=%d capped=%d",
            source != NULL ? source : "",
            today,
            seller_transfer_limit,
            seller_count,
            available_seller_count,
            capped_sellers);
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > 200000) {
        return 0;
    }
    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        return 0;
    }
    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (snapshot == NULL) {
        return 0;
    }
    SIZE_T bytes_read = 0u;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
    uint32_t buyer_team_ids[KBO_INDEPENDENT_ACQUISITION_MAX_BUYERS] = {0};
    int scanned = 0;
    int unreadable = 0;
    int buyer_count = collect_kbo_league_team_ids(
        kbo_league_id,
        buyer_team_ids,
        KBO_INDEPENDENT_ACQUISITION_MAX_BUYERS,
        &scanned,
        &unreadable);
    if (!kbo_independent_acquisition_claim_daily_run(today)) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        return 0;
    }

    int requested = 0;
    int refreshed_pending = 0;
    int considered_buyers = 0;
    int skipped_human = 0;
    for (int i = 0; i < buyer_count; i++) {
        uint32_t buyer_team_id = buyer_team_ids[i];
        if (kbo_team_is_human_controlled(buyer_team_id, "independent_acquisition_ai")) {
            skipped_human++;
            continue;
        }

        uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
        if (buyer_team == NULL || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        KboIndependentAcquisitionBuyerState buyer;
        kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
        if (buyer.team_id == 0u) {
            continue;
        }
        considered_buyers++;

        KboIndependentAcquisitionCandidate candidate;
        if (available_seller_count <= 0
                || !kbo_independent_acquisition_choose_candidate_for_buyer(
                snapshot,
                player_count,
                available_sellers,
                available_seller_count,
                &buyer,
                &candidate)) {
            continue;
        }

        const KboIndependentFuturesTeamLeague* seller = NULL;
        for (int s = 0; s < available_seller_count; s++) {
            if (available_sellers[s].team_id == candidate.seller_team_id) {
                seller = &available_sellers[s];
                break;
            }
        }
        if (seller == NULL) {
            continue;
        }

        int existing_request = kbo_independent_acquisition_request_exists(
            today / 10000u,
            buyer.team_id,
            candidate.seller_team_id,
            candidate.player_id);
        int request_available = existing_request
            || kbo_independent_acquisition_append_request(today, &candidate, &buyer, seller, source);
        if (request_available) {
            char request_score_text[32] = {0};
            snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)candidate.request_score);
            kbo_record_custom_foreign_pending_offer(
                buyer.team_id,
                (uint8_t*)candidate.player_ptr,
                today);
            if (existing_request) {
                refreshed_pending++;
            } else {
                requested++;
            }
            kbo_log_runtimef(
                "independent acquisition AI request source=%s action=%s buyer=%u seller=%u seller_csv=%s player=%u score=%s value=%d cash_cost=%d cash_available=%d effective=%u->%u limit=%u slot=%s",
                source != NULL ? source : "",
                existing_request ? "refresh_pending" : "new",
                buyer.team_id,
                candidate.seller_team_id,
                seller->team_csv_id,
                candidate.player_id,
                request_score_text,
                candidate.value_score,
                kbo_independent_acquisition_cash_cost_for_player((uint8_t*)candidate.player_ptr),
                buyer.cash_available,
                candidate.effective_before,
                candidate.effective_after,
                candidate.effective_limit,
                candidate.slot_type != 0u ? kbo_foreign_injury_slot_label(candidate.slot_type) : "none");
        }
    }

    kbo_log_runtimef(
        "independent acquisition AI summary source=%s today=%u requested=%d refreshed_pending=%d buyers=%d skipped_human=%d sellers=%d available_sellers=%d capped_sellers=%d seller_transfer_limit=%d player_count=%d team_scanned=%d team_unreadable=%d",
        source != NULL ? source : "",
        today,
        requested,
        refreshed_pending,
        considered_buyers,
        skipped_human,
        seller_count,
        available_seller_count,
        capped_sellers,
        seller_transfer_limit,
        player_count,
        scanned,
        unreadable);
    int transferred = kbo_run_independent_team_acquisition_seller_ai(
        today,
        snapshot,
        player_count,
        source);
    HeapFree(GetProcessHeap(), 0, snapshot);
    return requested + transferred;
}
