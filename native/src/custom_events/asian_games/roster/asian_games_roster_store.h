#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_ROSTER_STORE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_ROSTER_STORE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_get_asian_games_roster_csv_path(char* out, size_t out_size);
int kbo_get_legacy_asian_games_roster_csv_path(char* out, size_t out_size);
void kbo_clear_asian_games_roster_memory(const char* source);
void kbo_clear_asian_games_roster_if_save_changed(const char* source);
int kbo_save_asian_games_roster_csv(const char* source);
int kbo_load_asian_games_roster_csv(const char* source);

#endif
