#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_IMPORT_AND_LOAD_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_IMPORT_AND_LOAD_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_import_asian_games_schedule_seed_file_locked(const char* path, const char* source);
void kbo_ensure_asian_games_schedule_seeds_loaded(void);

#endif
