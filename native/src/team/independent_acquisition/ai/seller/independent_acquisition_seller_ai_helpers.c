#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_seller_ai_helpers.h"

#include <stdint.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../runtime_memory/runtime_memory.h"

int64_t kbo_independent_acquisition_seller_fit_score(
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

uint32_t kbo_independent_acquisition_seller_tiebreaker(
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

int kbo_independent_acquisition_seller_pacing_deferred(
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

int kbo_independent_acquisition_seller_abort_if_save(
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

uint32_t kbo_independent_acquisition_seller_effective_season(uint32_t today)
{
    uint32_t open_date = kbo_independent_team_acquisition_window_open_date();
    if (open_date != 0u && today >= open_date) {
        return open_date / 10000u;
    }
    return today / 10000u;
}

