#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
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

static uint32_t kbo_independent_acquisition_date_serial(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return 0u;
    }
    return kbo_date_serial(
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static int kbo_independent_acquisition_seller_pacing_deferred(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int seller_transfers,
    int seller_transfer_limit,
    uint32_t* out_window_age_days,
    uint32_t* out_target_day,
    uint32_t* out_request_age_days,
    uint32_t* out_days_remaining)
{
    if (request == NULL || seller_transfer_limit <= 1 || seller_transfers < 0) {
        return 0;
    }

    uint32_t today_serial = kbo_independent_acquisition_date_serial(today);
    uint32_t open_serial = kbo_independent_acquisition_date_serial(
        kbo_independent_team_acquisition_window_open_date());
    uint32_t request_serial = kbo_independent_acquisition_date_serial(request->date);
    if (today_serial == 0u || open_serial == 0u || today_serial < open_serial) {
        return 0;
    }

    uint32_t window_age_days = today_serial - open_serial;
    uint32_t request_age_days =
        request_serial != 0u && today_serial >= request_serial ? today_serial - request_serial : 0u;
    uint32_t window_days =
        kbo_foreign_player_policy()->pending_offer_ttl_days > 0
            ? (uint32_t)kbo_foreign_player_policy()->pending_offer_ttl_days
            : 45u;
    uint32_t days_remaining = window_age_days < window_days ? window_days - window_age_days : 0u;
    uint32_t remaining_transfers = seller_transfer_limit > seller_transfers
        ? (uint32_t)(seller_transfer_limit - seller_transfers)
        : 0u;
    if (remaining_transfers <= 1u || days_remaining <= remaining_transfers * 3u) {
        return 0;
    }

    uint32_t pacing_span = window_days > 10u ? window_days - 6u : window_days;
    uint32_t base_day = 2u + ((uint32_t)seller_transfers * pacing_span) / (uint32_t)seller_transfer_limit;
    uint32_t jitter = kbo_independent_acquisition_seller_tiebreaker(
        request->season,
        request) % 4u;
    uint32_t target_day = base_day + jitter;
    if (request_age_days >= 10u && target_day > 2u) {
        target_day -= 2u;
    }
    if (request_age_days >= 18u) {
        return 0;
    }

    if (out_window_age_days != NULL) {
        *out_window_age_days = window_age_days;
    }
    if (out_target_day != NULL) {
        *out_target_day = target_day;
    }
    if (out_request_age_days != NULL) {
        *out_request_age_days = request_age_days;
    }
    if (out_days_remaining != NULL) {
        *out_days_remaining = days_remaining;
    }
    return window_age_days < target_day;
}

static int kbo_independent_acquisition_seller_abort_if_save(
    const char* source,
    const char* stage,
    uint32_t today)
{
    if (!kbo_runtime_save_in_progress()) {
        return 0;
    }

    kbo_log_runtimef(
        "independent acquisition seller AI aborted source=%s reason=save_in_progress stage=%s today=%u",
        source != NULL ? source : "",
        stage != NULL ? stage : "",
        today);
    return 1;
}

static uint32_t kbo_independent_acquisition_seller_effective_season(uint32_t today)
{
    uint32_t open_date = kbo_independent_team_acquisition_window_open_date();
    if (open_date != 0u && today >= open_date) {
        return open_date / 10000u;
    }
    return today / 10000u;
}

int kbo_run_independent_team_acquisition_seller_ai(
    uint32_t today,
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const char* source)
{
    static volatile LONG seller_ai_running = 0;
    if (InterlockedCompareExchange(&seller_ai_running, 1, 0) != 0) {
        kbo_log_runtimef(
            "independent acquisition seller AI skipped source=%s today=%u reason=already_running",
            source != NULL ? source : "",
            today);
        return 0;
    }

    if (kbo_independent_acquisition_seller_abort_if_save(source, "after_lock", today)) {
        InterlockedExchange(&seller_ai_running, 0);
        return 0;
    }

    KboIndependentAcquisitionQueuedRequest queue[KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE];
    uint32_t season = kbo_independent_acquisition_seller_effective_season(today);
    int request_count = kbo_independent_acquisition_load_requests(
        season,
        queue,
        KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE);
    if (request_count <= 0) {
        InterlockedExchange(&seller_ai_running, 0);
        return 0;
    }

    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    int decided = 0;
    int transferred = 0;
    int limit_blocked = 0;
    int pacing_deferred = 0;
    int abort_for_save = 0;
    for (int i = 0; i < request_count; i++) {
        if (kbo_independent_acquisition_seller_abort_if_save(source, "request_loop", today)) {
            abort_for_save = 1;
            break;
        }
        KboIndependentAcquisitionQueuedRequest* group = &queue[i];
        if (group->player_id == 0u) {
            continue;
        }
        int seller_transfers = kbo_independent_acquisition_transferred_count(
            group->season,
            group->seller_team_id);
        int seller_limit_reached = seller_transfers >= seller_transfer_limit;
        if (seller_limit_reached) {
            limit_blocked++;
        }
        uint32_t window_age_days = 0u;
        uint32_t target_day = 0u;
        uint32_t request_age_days = 0u;
        uint32_t days_remaining = 0u;
        int pacing_blocked = !seller_limit_reached
            && kbo_independent_acquisition_seller_pacing_deferred(
                today,
                group,
                seller_transfers,
                seller_transfer_limit,
                &window_age_days,
                &target_day,
                &request_age_days,
                &days_remaining);
        if (pacing_blocked) {
            pacing_deferred++;
            kbo_log_runtimef(
                "independent acquisition seller AI deferred source=%s reason=market_pacing seller=%u player=%u buyer=%u seller_transfers=%d seller_transfer_limit=%d window_age_days=%u target_day=%u request_age_days=%u days_remaining=%u",
                source != NULL ? source : "",
                group->seller_team_id,
                group->player_id,
                group->buyer_team_id,
                seller_transfers,
                seller_transfer_limit,
                window_age_days,
                target_day,
                request_age_days,
                days_remaining);
            for (int j = i + 1; j < request_count; j++) {
                if (queue[j].player_id == group->player_id
                        && queue[j].seller_team_id == group->seller_team_id) {
                    queue[j].player_id = 0u;
                }
            }
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
            if ((j & 7) == 0
                    && kbo_independent_acquisition_seller_abort_if_save(source, "buyer_fit_loop", today)) {
                abort_for_save = 1;
                break;
            }
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
        if (abort_for_save) {
            break;
        }
        KboIndependentAcquisitionQueuedRequest selected = *best;
        for (int j = i + 1; j < request_count; j++) {
            if (queue[j].player_id == group->player_id
                    && queue[j].seller_team_id == group->seller_team_id) {
                queue[j].player_id = 0u;
            }
        }

        uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(selected.buyer_team_id, 1);
        uint8_t* seller_team = find_kbo_team_by_numeric_id_any_league(selected.seller_team_id, 1);
        int moved = 0;
        int cash_charged = 0;
        int32_t old_cash = 0;
        int32_t new_cash = 0;
        int32_t cash_cost = selected.cash_cost;
        if (cash_cost <= 0 && player != NULL) {
            cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
            selected.cash_cost = cash_cost;
        }
        if (kbo_independent_acquisition_seller_abort_if_save(source, "before_assignment", today)) {
            abort_for_save = 1;
            break;
        }
        if (!seller_limit_reached
                && !pacing_blocked
                && player != NULL
                && buyer_team != NULL
                && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                && memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)
                && kbo_independent_acquisition_player_status_ok(player)
                && kbo_player_current_assignment_matches_team_or_affiliate(player, selected.seller_team_id)
                && !kbo_player_current_assignment_matches_team_or_affiliate(player, selected.buyer_team_id)
                && kbo_independent_acquisition_team_has_cash(buyer_team, cash_cost)) {
            int pre = 0;
            int reg = 0;
            int attach = 0;
            uint32_t buyer_league_id = *(uint32_t*)(buyer_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            kbo_assign_player_to_team_like_ootp(player, buyer_team, buyer_league_id, &pre, &reg, &attach);
            moved = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == selected.buyer_team_id;
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
                        selected.buyer_team_id,
                        selected.player_id,
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
                selected.player_id,
                selected.buyer_team_id,
                selected.seller_team_id,
                cash_cost,
                source);
        }

        if (kbo_independent_acquisition_seller_abort_if_save(source, "before_append_decision", today)) {
            abort_for_save = 1;
            break;
        }
        if (kbo_independent_acquisition_append_decision(today, &selected, moved, old_cash, new_cash, source)) {
            decided++;
            if (moved) {
                transferred++;
            }
            char request_score_text[32] = {0};
            snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)best->request_score);
            kbo_log_runtimef(
                "independent acquisition seller AI decision source=%s seller=%u player=%u buyer=%u score=%s adjusted_fit=%lld buyer_transfers=%d tiebreaker=%u cash_cost=%d old_cash=%d new_cash=%d transferred=%d seller_transfers=%d seller_transfer_limit=%d",
                source != NULL ? source : "",
                selected.seller_team_id,
                selected.player_id,
                selected.buyer_team_id,
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
        "independent acquisition seller AI summary source=%s today=%u queued=%d decided=%d transferred=%d limit_blocked=%d pacing_deferred=%d seller_transfer_limit=%d aborted_for_save=%d",
        source != NULL ? source : "",
        today,
        request_count,
        decided,
        transferred,
        limit_blocked,
        pacing_deferred,
        seller_transfer_limit,
        abort_for_save);
    InterlockedExchange(&seller_ai_running, 0);
    return transferred;
}
