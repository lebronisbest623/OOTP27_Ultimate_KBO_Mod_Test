#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_BODY_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_BODY_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_asian_games_append_player_blurb(char* out, size_t out_size, size_t* used, KboAsianGamesRosterEntry* entry, LONG display_index, LONG display_count);
int kbo_asian_games_append_roster_line(char* out, size_t out_size, size_t* used, LONG index, KboAsianGamesRosterEntry* entry);
KboAsianGamesRosterEntry* kbo_asian_games_choose_captain(void);
void kbo_build_asian_games_news_body(char* out, size_t out_size, const char* template_prefix, const char* lead, const char* source);

#endif
