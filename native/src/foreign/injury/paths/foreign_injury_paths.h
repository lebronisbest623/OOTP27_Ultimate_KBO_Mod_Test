#ifndef KBOFIX_SRC_FOREIGN_INJURY_FOREIGN_INJURY_PATHS_H_
#define KBOFIX_SRC_FOREIGN_INJURY_FOREIGN_INJURY_PATHS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_get_foreign_injury_replacement_path(char* out, size_t out_size);
int kbo_get_save_foreign_injury_replacement_seed_path(char* out, size_t out_size);
int kbo_get_global_foreign_injury_replacement_seed_path(char* out, size_t out_size);

#endif
