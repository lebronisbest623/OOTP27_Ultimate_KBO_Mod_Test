#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai.h"
#include "ai/independent_acquisition_ai_internal.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

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
#include "../assignment/assignment/team_assignment.h"
#include "../assignment/org_query/team_org_assignment_query.h"
#include "../classification/team_classification.h"
#include "../control/team_human_control.h"
#include "../lookup/team_lookup.h"
#include "independent_acquisition_window.h"

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

    return today_serial - open_serial <= (uint32_t)kbo_foreign_player_policy()->pending_offer_ttl_days;
}

static int kbo_run_independent_team_acquisition_seller_ai(
    uint32_t today,
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const char* source)
{
    KboIndependentAcquisitionQueuedRequest queue[KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE];
    int request_count = kbo_independent_acquisition_load_requests(
        today / 10000u,
        queue,
        KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE);
    if (request_count <= 0) {
        return 0;
    }

    int decided = 0;
    int transferred = 0;
    for (int i = 0; i < request_count; i++) {
        KboIndependentAcquisitionQueuedRequest* best = &queue[i];
        if (best->player_id == 0u) {
            continue;
        }
        for (int j = i + 1; j < request_count; j++) {
            if (queue[j].player_id == best->player_id
                    && queue[j].seller_team_id == best->seller_team_id
                    && queue[j].request_score > best->request_score) {
                best = &queue[j];
            }
        }
        for (int j = i + 1; j < request_count; j++) {
            if (queue[j].player_id == best->player_id
                    && queue[j].seller_team_id == best->seller_team_id) {
                queue[j].player_id = 0u;
            }
        }

        uintptr_t player_ptr = kbo_independent_acquisition_find_player_snapshot(
            player_snapshot,
            player_count,
            best->player_id);
        uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(best->buyer_team_id, 1);
        uint8_t* player = (uint8_t*)player_ptr;
        int moved = 0;
        int32_t old_cash = 0;
        int32_t new_cash = 0;
        int32_t cash_cost = best->cash_cost;
        if (cash_cost <= 0 && player != NULL) {
            cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
            best->cash_cost = cash_cost;
        }
        if (player != NULL
                && buyer_team != NULL
                && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                && memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)
                && kbo_independent_acquisition_player_status_ok(player)
                && kbo_player_current_assignment_matches_team_or_affiliate(player, best->seller_team_id)
                && !kbo_player_current_assignment_matches_team_or_affiliate(player, best->buyer_team_id)
                && kbo_independent_acquisition_team_has_cash(buyer_team, cash_cost)) {
            int pre = 0;
            int reg = 0;
            int attach = 0;
            uint32_t buyer_league_id = *(uint32_t*)(buyer_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            kbo_assign_player_to_team_like_ootp(player, buyer_team, buyer_league_id, &pre, &reg, &attach);
            moved = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == best->buyer_team_id;
            if (moved
                    && !kbo_independent_acquisition_charge_team_cash(
                        buyer_team,
                        cash_cost,
                        &old_cash,
                        &new_cash)) {
                kbo_log_runtimef(
                    "independent acquisition seller AI cash charge failed source=%s buyer=%u player=%u cost=%d",
                    source != NULL ? source : "",
                    best->buyer_team_id,
                    best->player_id,
                    cash_cost);
            }
        }

        if (kbo_independent_acquisition_append_decision(today, best, moved, old_cash, new_cash, source)) {
            decided++;
            if (moved) {
                transferred++;
            }
            char request_score_text[32] = {0};
            snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)best->request_score);
            kbo_log_runtimef(
                "independent acquisition seller AI decision source=%s seller=%u player=%u buyer=%u score=%s cash_cost=%d old_cash=%d new_cash=%d transferred=%d",
                source != NULL ? source : "",
                best->seller_team_id,
                best->player_id,
                best->buyer_team_id,
                request_score_text,
                cash_cost,
                old_cash,
                new_cash,
                moved);
        }
    }

    kbo_log_runtimef(
        "independent acquisition seller AI summary source=%s today=%u queued=%d decided=%d transferred=%d",
        source != NULL ? source : "",
        today,
        request_count,
        decided,
        transferred);
    return transferred;
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
        if (!kbo_independent_acquisition_choose_candidate_for_buyer(
                snapshot,
                player_count,
                sellers,
                seller_count,
                &buyer,
                &candidate)) {
            continue;
        }

        const KboIndependentFuturesTeamLeague* seller = NULL;
        for (int s = 0; s < seller_count; s++) {
            if (sellers[s].team_id == candidate.seller_team_id) {
                seller = &sellers[s];
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
        "independent acquisition AI summary source=%s today=%u requested=%d refreshed_pending=%d buyers=%d skipped_human=%d sellers=%d player_count=%d team_scanned=%d team_unreadable=%d",
        source != NULL ? source : "",
        today,
        requested,
        refreshed_pending,
        considered_buyers,
        skipped_human,
        seller_count,
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
