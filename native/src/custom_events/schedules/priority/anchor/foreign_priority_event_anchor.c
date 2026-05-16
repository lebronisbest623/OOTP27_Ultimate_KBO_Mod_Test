#include "../foreign_priority_event_schedule_internal.h"

#include "../../../runtime/common/custom_events_common.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/waiver_window/state/foreign_waiver_window_state.h"

uint32_t kbo_recent_phase_transition_offseason_anchor(uint32_t league_id, uint32_t today_yyyymmdd)
{
    if (league_id == 0u
            || today_yyyymmdd == 0u
            || g_kbo_custom_event_last_offseason_transition_anchor == 0u
            || g_kbo_custom_event_last_offseason_transition_anchor != g_kbo_foreign_priority_last_scheduled_date
            || g_kbo_custom_event_last_phase_league_id != league_id
            || g_kbo_custom_event_last_seen_league_phase > 1u) {
        return 0u;
    }

    uint32_t anchor = g_kbo_custom_event_last_offseason_transition_anchor;
    if (anchor > today_yyyymmdd) {
        return 0u;
    }

    uint32_t anchor_serial = kbo_date_serial(anchor / 10000u, (anchor / 100u) % 100u, anchor % 100u);
    uint32_t today_serial = kbo_date_serial(today_yyyymmdd / 10000u, (today_yyyymmdd / 100u) % 100u, today_yyyymmdd % 100u);
    if (anchor_serial == 0u || today_serial == 0u || today_serial < anchor_serial) {
        return 0u;
    }
    uint32_t max_age_days = (uint32_t)kbo_foreign_player_policy()->phase_transition_anchor_max_age_days;
    if (today_serial - anchor_serial > max_age_days) {
        return 0u;
    }
    return anchor;
}

uint32_t kbo_recent_foreign_waiver_marker_anchor(uint32_t today_yyyymmdd, const char* source)
{
    uint32_t marker = g_kbo_foreign_waiver_last_seen_yyyymmdd;
    if (marker == 0u || today_yyyymmdd == 0u || marker > today_yyyymmdd) {
        return 0u;
    }

    uint32_t marker_serial = kbo_date_serial(
        marker / 10000u,
        (marker / 100u) % 100u,
        marker % 100u);
    uint32_t today_serial = kbo_date_serial(
        today_yyyymmdd / 10000u,
        (today_yyyymmdd / 100u) % 100u,
        today_yyyymmdd % 100u);
    if (marker_serial == 0u || today_serial == 0u || today_serial < marker_serial) {
        return 0u;
    }

    uint32_t age_days = today_serial - marker_serial;
    uint32_t max_age_days = (uint32_t)kbo_foreign_player_policy()->foreign_priority_marker_max_age_days;
    if (age_days > max_age_days) {
        return 0u;
    }

    kbo_log_runtimef(
        "KBO custom event schedule fallback source=%s reason=recent_foreign_waiver_marker today=%u season_end=%u age_days=%u",
        source != NULL ? source : "",
        today_yyyymmdd,
        marker,
        age_days);
    return marker;
}

uint32_t kbo_custom_event_add_months_yyyymmdd(uint32_t yyyymmdd, uint32_t months)
{
    uint32_t result = yyyymmdd;
    for (uint32_t i = 0u; i < months; i++) {
        result = kbo_add_one_month_yyyymmdd(result);
        if (result == 0u) {
            return 0u;
        }
    }
    return result;
}

