#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_PROJECTED_POLICY_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_PROJECTED_POLICY_H_

#include <stdint.h>

#define KBO_ASIAN_GAMES_PROJECTED_STATUS_MAX 32u
#define KBO_ASIAN_GAMES_PROJECTED_NOTES_MAX 256u

typedef struct KboAsianGamesProjectedPolicy {
    uint32_t projected_start_year;
    uint32_t projected_end_year;
    uint32_t cycle_years;
    uint32_t start_day_base;
    uint32_t start_day_span;
    uint32_t duration_days_base;
    uint32_t duration_days_span;
    uint32_t selection_lead_days_base;
    uint32_t selection_lead_days_span;
    uint32_t selection_fallback_month_day;
    uint32_t start_day_hash_salt;
    uint32_t duration_hash_salt;
    uint32_t selection_lead_hash_salt;
    uint32_t host_hash_salt;
    uint32_t host_max_count;
    uint32_t host_city_cooldown_years;
    uint32_t host_country_cooldown_years;
    uint32_t host_fallback_city_cooldown_years;
    uint32_t host_fallback_country_cooldown_years;
    char projected_status[KBO_ASIAN_GAMES_PROJECTED_STATUS_MAX];
    char projected_notes[KBO_ASIAN_GAMES_PROJECTED_NOTES_MAX];
} KboAsianGamesProjectedPolicy;

void kbo_load_asian_games_projected_policy(KboAsianGamesProjectedPolicy* out);

#endif
