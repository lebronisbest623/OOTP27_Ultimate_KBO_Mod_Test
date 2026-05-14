#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../runtime/common/custom_events_common.h"
#include "builtin_and_projected.h"
#include "policy/projected_policy.h"
#include <string.h>
#include "../../../core/dates/core_text_date.h"

int kbo_add_asian_games_schedule_seed_locked(const KboAsianGamesScheduleSeed* seed)
{
    if (seed == NULL || seed->year == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        if (g_kbo_asian_games_schedule_seeds[i].year == seed->year) {
            g_kbo_asian_games_schedule_seeds[i] = *seed;
            return 0;
        }
    }
    if (g_kbo_asian_games_schedule_seed_count >= KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX) {
        return 0;
    }
    g_kbo_asian_games_schedule_seeds[g_kbo_asian_games_schedule_seed_count++] = *seed;
    return 1;
}

int kbo_asian_games_year_is_projected(uint32_t year)
{
    KboAsianGamesProjectedPolicy policy;
    kbo_load_asian_games_projected_policy(&policy);
    return year >= policy.projected_start_year
        && year <= policy.projected_end_year
        && ((year - policy.projected_start_year) % policy.cycle_years) == 0u;
}

uint32_t kbo_next_projected_asian_games_year(uint32_t from_year)
{
    KboAsianGamesProjectedPolicy policy;
    kbo_load_asian_games_projected_policy(&policy);
    if (from_year > policy.projected_end_year) {
        return 0u;
    }
    if (from_year <= policy.projected_start_year) {
        return policy.projected_start_year;
    }
    uint32_t delta = from_year - policy.projected_start_year;
    uint32_t year = policy.projected_start_year
        + ((delta + policy.cycle_years - 1u) / policy.cycle_years) * policy.cycle_years;
    return year <= policy.projected_end_year ? year : 0u;
}

uint32_t kbo_asian_games_projected_hash(uint32_t year, uint32_t salt)
{
    uint32_t value = year ^ (salt * 0x9E3779B9u);
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

static uint32_t kbo_subtract_days_yyyymmdd(uint32_t yyyymmdd, uint32_t days)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year == 0u || month == 0u || month > 12u || day == 0u || day > (uint32_t)kbo_days_in_month(year, month)) {
        return 0u;
    }
    while (days > 0u) {
        if (day > 1u) {
            day--;
        } else if (month > 1u) {
            month--;
            day = (uint32_t)kbo_days_in_month(year, month);
        } else {
            if (year == 0u) {
                return 0u;
            }
            year--;
            month = 12u;
            day = (uint32_t)kbo_days_in_month(year, month);
        }
        days--;
    }
    return year * 10000u + month * 100u + day;
}

void kbo_build_projected_asian_games_schedule(uint32_t year, KboAsianGamesScheduleSeed* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!kbo_asian_games_year_is_projected(year)) {
        return;
    }
    KboAsianGamesProjectedPolicy policy;
    kbo_load_asian_games_projected_policy(&policy);
    uint32_t start_day = policy.start_day_base
        + (kbo_asian_games_projected_hash(year, policy.start_day_hash_salt) % policy.start_day_span);
    uint32_t duration_days = policy.duration_days_base
        + (kbo_asian_games_projected_hash(year, policy.duration_hash_salt) % policy.duration_days_span);
    uint32_t selection_lead_days = policy.selection_lead_days_base
        + (kbo_asian_games_projected_hash(year, policy.selection_lead_hash_salt) % policy.selection_lead_days_span);
    uint32_t tournament_start = year * 10000u + 900u + start_day;
    uint32_t tournament_end = kbo_add_days_yyyymmdd(tournament_start, duration_days);
    uint32_t selection_date = kbo_subtract_days_yyyymmdd(tournament_start, selection_lead_days);
    if (selection_date / 10000u != year) {
        selection_date = year * 10000u + policy.selection_fallback_month_day;
    }
    out->year = year;
    kbo_choose_projected_asian_games_host(
        year,
        out->host_city,
        sizeof(out->host_city),
        out->host_country,
        sizeof(out->host_country));
    kbo_asian_games_schedule_copy_text(out->status, sizeof(out->status), policy.projected_status);
    out->tournament_start = tournament_start;
    out->tournament_end = tournament_end;
    out->selection_date = selection_date;
    out->departure_date = out->tournament_start;
    out->final_date = out->tournament_end;
    out->auto_schedule = 1u;
    kbo_asian_games_schedule_copy_text(
        out->notes,
        sizeof(out->notes),
        policy.projected_notes);
}
