#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_HANDLERS_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_HANDLERS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_handle_asian_games_selection_event(uint32_t event_yyyymmdd, const char* source);
int kbo_handle_asian_games_departure_event(uint32_t event_yyyymmdd, const char* source);
int kbo_handle_asian_games_final_event(uint32_t event_yyyymmdd, const char* source);

#endif
