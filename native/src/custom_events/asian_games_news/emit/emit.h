#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_EMIT_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_EMIT_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_emit_asian_games_news(uint32_t event_yyyymmdd, const char* template_prefix, const char* source);
uint32_t kbo_asian_games_effective_action_date(uint32_t event_yyyymmdd);
int kbo_emit_asian_games_replacement_news(uint32_t event_yyyymmdd, KboAsianGamesRosterEntry* old_entry, KboAsianGamesRosterEntry* new_entry, const char* source);

#endif
