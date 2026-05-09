#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_QUERY_HELPERS_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_QUERY_HELPERS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_get_asian_games_schedule_for_year(uint32_t year, KboAsianGamesScheduleSeed* out);
int kbo_get_next_asian_games_schedule(uint32_t from_year, KboAsianGamesScheduleSeed* out);
int kbo_asian_games_schedule_has_event_dates(const KboAsianGamesScheduleSeed* schedule);
int kbo_asian_games_schedule_auto_events_enabled(const KboAsianGamesScheduleSeed* schedule);
const char* kbo_asian_games_schedule_status_label(const KboAsianGamesScheduleSeed* schedule);

#endif
