#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_BUILTIN_AND_PROJECTED_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_BUILTIN_AND_PROJECTED_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_add_asian_games_schedule_seed_locked(const KboAsianGamesScheduleSeed* seed);
void kbo_add_builtin_asian_games_schedule_seed_locked(uint32_t year, const char* host_city, const char* host_country, const char* status, uint32_t tournament_start, uint32_t tournament_end, uint32_t selection_date, uint32_t departure_date, uint32_t final_date, const char* notes);
void kbo_add_builtin_asian_games_schedule_seeds_locked(void);
int kbo_asian_games_year_is_projected(uint32_t year);
uint32_t kbo_next_projected_asian_games_year(uint32_t from_year);
uint32_t kbo_asian_games_projected_hash(uint32_t year, uint32_t salt);
int kbo_choose_projected_asian_games_host(
    uint32_t year,
    char* out_city,
    size_t city_size,
    char* out_country,
    size_t country_size);
void kbo_build_projected_asian_games_schedule(uint32_t year, KboAsianGamesScheduleSeed* out);

#endif
