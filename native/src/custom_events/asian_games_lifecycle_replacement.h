#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_REPLACEMENT_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_REPLACEMENT_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_asian_games_find_replacement_for_entry(KboAsianGamesRosterEntry* old_entry, LONG old_index, KboAsianGamesRosterEntry* out_entry, uint32_t* out_org_id, const char* source);
int kbo_asian_games_replace_unavailable_players(uint32_t event_yyyymmdd, const char* source);

#endif
