#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../runtime/common/custom_events_common.h"
#include "builtin_and_projected.h"
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

void kbo_add_builtin_asian_games_schedule_seed_locked(
    uint32_t year,
    const char* host_city,
    const char* host_country,
    const char* status,
    uint32_t tournament_start,
    uint32_t tournament_end,
    uint32_t selection_date,
    uint32_t departure_date,
    uint32_t final_date,
    const char* notes)
{
    KboAsianGamesScheduleSeed seed;
    memset(&seed, 0, sizeof(seed));
    seed.year = year;
    kbo_asian_games_schedule_copy_text(seed.host_city, sizeof(seed.host_city), host_city);
    kbo_asian_games_schedule_copy_text(seed.host_country, sizeof(seed.host_country), host_country);
    kbo_asian_games_schedule_copy_text(seed.status, sizeof(seed.status), status);
    seed.tournament_start = tournament_start;
    seed.tournament_end = tournament_end;
    seed.selection_date = selection_date;
    seed.departure_date = departure_date;
    seed.final_date = final_date;
    seed.auto_schedule = 1u;
    kbo_asian_games_schedule_copy_text(seed.notes, sizeof(seed.notes), notes);
    kbo_add_asian_games_schedule_seed_locked(&seed);
}

void kbo_add_builtin_asian_games_schedule_seeds_locked(void)
{
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2026u,
        "Aichi-Nagoya",
        "Japan",
        "official",
        20260919u,
        20261004u,
        20260805u,
        20260919u,
        20261004u,
        "Built-in Aichi-Nagoya 2026 Asian Games schedule.");
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2031u,
        "Doha",
        "Qatar",
        "provisional",
        20311104u,
        20311119u,
        20310920u,
        20311104u,
        20311119u,
        "Built-in provisional Doha 2031 Asian Games schedule.");
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2035u,
        "Riyadh",
        "Saudi Arabia",
        "provisional",
        20351129u,
        20351214u,
        20351015u,
        20351129u,
        20351214u,
        "Built-in provisional Riyadh 2035 Asian Games schedule.");
}

int kbo_asian_games_year_is_projected(uint32_t year)
{
    return year >= 2039u && year <= 2200u && ((year - 2039u) % 4u) == 0u;
}

uint32_t kbo_next_projected_asian_games_year(uint32_t from_year)
{
    if (from_year > 2200u) {
        return 0u;
    }
    if (from_year <= 2039u) {
        return 2039u;
    }
    uint32_t delta = from_year - 2039u;
    uint32_t year = 2039u + ((delta + 3u) / 4u) * 4u;
    return year <= 2200u ? year : 0u;
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
    uint32_t start_day = 10u + (kbo_asian_games_projected_hash(year, 31u) % 19u);
    uint32_t duration_days = 14u + (kbo_asian_games_projected_hash(year, 43u) % 5u);
    uint32_t selection_lead_days = 42u + (kbo_asian_games_projected_hash(year, 59u) % 18u);
    uint32_t tournament_start = year * 10000u + 900u + start_day;
    uint32_t tournament_end = kbo_add_days_yyyymmdd(tournament_start, duration_days);
    uint32_t selection_date = kbo_subtract_days_yyyymmdd(tournament_start, selection_lead_days);
    if (selection_date / 10000u != year) {
        selection_date = year * 10000u + 805u;
    }
    out->year = year;
    kbo_choose_projected_asian_games_host(
        year,
        out->host_city,
        sizeof(out->host_city),
        out->host_country,
        sizeof(out->host_country));
    kbo_asian_games_schedule_copy_text(out->status, sizeof(out->status), "projected");
    out->tournament_start = tournament_start;
    out->tournament_end = tournament_end;
    out->selection_date = selection_date;
    out->departure_date = out->tournament_start;
    out->final_date = out->tournament_end;
    out->auto_schedule = 1u;
    kbo_asian_games_schedule_copy_text(
        out->notes,
        sizeof(out->notes),
        "Projected post-2035 Asian Games schedule; override this year in asian_games_schedule_seed.csv when official dates are known.");
}
