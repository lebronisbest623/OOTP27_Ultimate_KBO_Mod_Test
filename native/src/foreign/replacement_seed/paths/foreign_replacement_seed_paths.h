#ifndef KBOFIX_SRC_FOREIGN_REPLACEMENT_SEED_FOREIGN_REPLACEMENT_SEED_PATHS_H_
#define KBOFIX_SRC_FOREIGN_REPLACEMENT_SEED_FOREIGN_REPLACEMENT_SEED_PATHS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_get_save_foreign_replacement_players_seed_path(char* out, size_t out_size);
int kbo_get_global_foreign_replacement_players_seed_path(char* out, size_t out_size);
int kbo_get_save_foreign_replacement_players_resolved_path(char* out, size_t out_size);

#endif
