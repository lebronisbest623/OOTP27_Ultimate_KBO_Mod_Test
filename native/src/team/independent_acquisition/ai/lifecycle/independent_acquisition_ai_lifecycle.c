#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai_lifecycle.h"

#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/season/phase/season_phase.h"
#include "../../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../lookup/team_lookup.h"
#include "../../window/independent_acquisition_window.h"

static volatile LONG g_kbo_independent_acquisition_ai_last_run_date = 0;

int kbo_independent_acquisition_claim_daily_run(uint32_t today)
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

void kbo_independent_acquisition_release_daily_run(uint32_t today)
{
    if (today == 0u) {
        return;
    }

    InterlockedCompareExchange(
        &g_kbo_independent_acquisition_ai_last_run_date,
        0,
        (LONG)today);
}

int kbo_independent_acquisition_abort_if_save(
    const char* source,
    const char* stage,
    uint32_t today)
{
    if (!kbo_runtime_save_in_progress()) {
        return 0;
    }

    kbo_log_runtimef(
        "independent acquisition AI aborted source=%s reason=save_in_progress stage=%s today=%u",
        source != NULL ? source : "",
        stage != NULL ? stage : "",
        today);
    return 1;
}

int kbo_independent_acquisition_window_active(uint32_t today)
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

    int32_t ttl_days = kbo_foreign_player_policy()->pending_offer_ttl_days;
    uint32_t ttl = ttl_days > 0 ? (uint32_t)ttl_days : 0u;
    uint32_t age_days = today_serial - open_serial;
    if (age_days > ttl) {
        static uint32_t last_logged_expired_date = 0u;
        if (last_logged_expired_date != today) {
            last_logged_expired_date = today;
            kbo_log_runtimef(
                "independent acquisition AI window closed source=ttl today=%u open=%u age_days=%u ttl_days=%u",
                today,
                open_date,
                age_days,
                ttl);
        }
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    KboSeasonPhaseInfo phase_info;
    if (kbo_season_phase_resolve(league_id, today, 0u, &phase_info)
            && !kbo_season_phase_is_preseason_or_regular(phase_info.effective_phase)) {
        static uint32_t last_logged_phase_closed_date = 0u;
        if (last_logged_phase_closed_date != today) {
            last_logged_phase_closed_date = today;
            kbo_log_runtimef(
                "independent acquisition AI window closed source=effective_phase today=%u open=%u league=%u raw_phase=%u effective_phase=%u label=%s opening_day=%u corrected=%d",
                today,
                open_date,
                league_id,
                (unsigned)phase_info.raw_phase,
                (unsigned)phase_info.effective_phase,
                kbo_season_phase_label(phase_info.effective_phase),
                phase_info.opening_day,
                phase_info.corrected);
        }
        return 0;
    }

    return 1;
}

int kbo_independent_acquisition_buyer_has_pending_request(
    const KboIndependentAcquisitionQueuedRequest* requests,
    int request_count,
    uint32_t buyer_team_id)
{
    if (requests == NULL || request_count <= 0 || buyer_team_id == 0u) {
        return 0;
    }
    for (int i = 0; i < request_count; i++) {
        if (requests[i].buyer_team_id == buyer_team_id) {
            return 1;
        }
    }
    return 0;
}

uint32_t kbo_independent_acquisition_effective_season(uint32_t today)
{
    uint32_t open_date = kbo_independent_team_acquisition_window_open_date();
    if (open_date != 0u && today >= open_date) {
        return open_date / 10000u;
    }
    return today / 10000u;
}

