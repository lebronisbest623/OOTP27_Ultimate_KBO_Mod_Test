#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../assignment/assignment/team_assignment.h"
#include "../../../assignment/org_query/team_org_assignment_query.h"
#include "../../../lookup/team_lookup.h"

static int64_t kbo_independent_acquisition_seller_fit_score(
    const KboIndependentAcquisitionQueuedRequest* request,
    uint8_t* player,
    uint8_t* buyer_team,
    int32_t cash_cost)
{
    if (request == NULL
            || player == NULL
            || buyer_team == NULL
            || cash_cost <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_independent_acquisition_team_has_cash(buyer_team, cash_cost)) {
        return INT64_MIN;
    }

    KboIndependentAcquisitionBuyerState buyer;
    kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
    if (buyer.team_id == 0u || buyer.team_id != request->buyer_team_id) {
        return INT64_MIN;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    if (kbo_player_is_foreign_for_kbo_rights(player)) {
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            buyer.team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            return INT64_MIN;
        }
    }

    return kbo_independent_acquisition_score_candidate_for_buyer(
        &buyer,
        player,
        effective_before,
        effective_limit);
}

static uint32_t kbo_independent_acquisition_seller_tiebreaker(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request)
{
    if (request == NULL) {
        return 0u;
    }

    uint32_t value = today
        ^ (request->buyer_team_id * 1103515245u)
        ^ (request->seller_team_id * 2246822519u)
        ^ (request->player_id * 3266489917u)
        ^ (request->date * 668265263u);
    value ^= value >> 16;
    value *= 2246822519u;
    value ^= value >> 13;
    value *= 3266489917u;
    value ^= value >> 16;
    return value;
}

int kbo_run_independent_team_acquisition_seller_ai(
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

    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    int decided = 0;
    int transferred = 0;
    int limit_blocked = 0;
    for (int i = 0; i < request_count; i++) {
        KboIndependentAcquisitionQueuedRequest* group = &queue[i];
        if (group->player_id == 0u) {
            continue;
        }
        uintptr_t player_ptr = kbo_independent_acquisition_find_player_snapshot(
            player_snapshot,
            player_count,
            group->player_id);
        uint8_t* player = (uint8_t*)player_ptr;
        KboIndependentAcquisitionQueuedRequest* best = group;
        int64_t best_fit_score = INT64_MIN;
        int best_buyer_transfers = 0;
        uint32_t best_tiebreaker = 0u;
        for (int j = i; j < request_count; j++) {
            if (queue[j].player_id != group->player_id
                    || queue[j].seller_team_id != group->seller_team_id) {
                continue;
            }
            uint8_t* candidate_team = find_kbo_team_by_numeric_id_any_league(queue[j].buyer_team_id, 1);
            int32_t candidate_cash_cost = queue[j].cash_cost;
            if (candidate_cash_cost <= 0 && player != NULL) {
                candidate_cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
                queue[j].cash_cost = candidate_cash_cost;
            }
            int64_t fit_score = kbo_independent_acquisition_seller_fit_score(
                &queue[j],
                player,
                candidate_team,
                candidate_cash_cost);
            int buyer_transfers = kbo_independent_acquisition_buyer_transferred_count(
                queue[j].season,
                queue[j].buyer_team_id);
            int64_t adjusted_fit_score = fit_score == INT64_MIN
                ? INT64_MIN
                : fit_score - ((int64_t)buyer_transfers * 1000000ll);
            if (adjusted_fit_score != INT64_MIN && today >= queue[j].date) {
                uint32_t today_serial = kbo_date_serial(
                    today / 10000u,
                    (today / 100u) % 100u,
                    today % 100u);
                uint32_t request_serial = kbo_date_serial(
                    queue[j].date / 10000u,
                    (queue[j].date / 100u) % 100u,
                    queue[j].date % 100u);
                if (today_serial != 0u && request_serial != 0u && today_serial >= request_serial) {
                    uint32_t request_age_days = today_serial - request_serial;
                    if (request_age_days > 30u) {
                        request_age_days = 30u;
                    }
                    adjusted_fit_score += (int64_t)request_age_days * 2500ll;
                }
            }
            uint32_t tiebreaker = kbo_independent_acquisition_seller_tiebreaker(today, &queue[j]);
            int best_penalty = best_buyer_transfers;
            if (adjusted_fit_score > best_fit_score
                    || (adjusted_fit_score == best_fit_score && buyer_transfers < best_penalty)
                    || (adjusted_fit_score == best_fit_score
                        && buyer_transfers == best_penalty
                        && queue[j].request_score > best->request_score)
                    || (adjusted_fit_score == best_fit_score
                        && buyer_transfers == best_penalty
                        && queue[j].request_score == best->request_score
                        && tiebreaker > best_tiebreaker)) {
                best = &queue[j];
                best_fit_score = adjusted_fit_score;
                best_buyer_transfers = buyer_transfers;
                best_tiebreaker = tiebreaker;
            }
        }
        for (int j = i + 1; j < request_count; j++) {
            if (queue[j].player_id == group->player_id
                    && queue[j].seller_team_id == group->seller_team_id) {
                queue[j].player_id = 0u;
            }
        }

        uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(best->buyer_team_id, 1);
        uint8_t* seller_team = find_kbo_team_by_numeric_id_any_league(best->seller_team_id, 1);
        int moved = 0;
        int cash_charged = 0;
        int32_t old_cash = 0;
        int32_t new_cash = 0;
        int32_t cash_cost = best->cash_cost;
        if (cash_cost <= 0 && player != NULL) {
            cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
            best->cash_cost = cash_cost;
        }
        int seller_transfers = kbo_independent_acquisition_transferred_count(
            best->season,
            best->seller_team_id);
        int seller_limit_reached = seller_transfers >= seller_transfer_limit;
        if (seller_limit_reached) {
            limit_blocked++;
        }
        if (!seller_limit_reached
                && player != NULL
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
            if (moved) {
                cash_charged = kbo_independent_acquisition_charge_team_cash(
                        buyer_team,
                        cash_cost,
                        &old_cash,
                        &new_cash);
                if (!cash_charged) {
                    kbo_log_runtimef(
                        "independent acquisition seller AI cash charge failed source=%s buyer=%u player=%u cost=%d",
                        source != NULL ? source : "",
                        best->buyer_team_id,
                        best->player_id,
                        cash_cost);
                }
            }
        }
        if (moved && cash_charged) {
            kbo_emit_independent_acquisition_transfer_news(
                today,
                player,
                buyer_team,
                seller_team,
                best->player_id,
                best->buyer_team_id,
                best->seller_team_id,
                cash_cost,
                source);
        }

        if (kbo_independent_acquisition_append_decision(today, best, moved, old_cash, new_cash, source)) {
            decided++;
            if (moved) {
                transferred++;
            }
            char request_score_text[32] = {0};
            snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)best->request_score);
            kbo_log_runtimef(
                "independent acquisition seller AI decision source=%s seller=%u player=%u buyer=%u score=%s adjusted_fit=%lld buyer_transfers=%d tiebreaker=%u cash_cost=%d old_cash=%d new_cash=%d transferred=%d seller_transfers=%d seller_transfer_limit=%d",
                source != NULL ? source : "",
                best->seller_team_id,
                best->player_id,
                best->buyer_team_id,
                request_score_text,
                (long long)best_fit_score,
                best_buyer_transfers,
                best_tiebreaker,
                cash_cost,
                old_cash,
                new_cash,
                moved,
                seller_transfers,
                seller_transfer_limit);
        }
    }

    kbo_log_runtimef(
        "independent acquisition seller AI summary source=%s today=%u queued=%d decided=%d transferred=%d limit_blocked=%d seller_transfer_limit=%d",
        source != NULL ? source : "",
        today,
        request_count,
        decided,
        transferred,
        limit_blocked,
        seller_transfer_limit);
    return transferred;
}
