#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_DEPARTURE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_DEPARTURE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void kbo_record_asian_games_restricted_reason(KboAsianGamesRosterEntry* entry, uint32_t event_yyyymmdd, uint32_t league_id, uint32_t team_id, const char* source);
int kbo_asian_games_depart_selected_players(uint32_t event_yyyymmdd, const char* source);

#endif
