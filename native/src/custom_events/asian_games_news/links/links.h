#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_LINKS_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_LINKS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void kbo_copy_asian_games_team_name(uint32_t team_id, char* out, size_t out_size);
void kbo_copy_asian_games_team_link(uint32_t team_id, char* out, size_t out_size);
void kbo_copy_asian_games_player_link(KboAsianGamesRosterEntry* entry, char* out, size_t out_size);

#endif
