#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_FINAL_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_FINAL_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_asian_games_finalize_selected_players(uint32_t event_yyyymmdd, const char* source);
int kbo_asian_games_roster_already_finalized(const char* source);

#endif
