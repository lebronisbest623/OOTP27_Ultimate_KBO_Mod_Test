#ifndef KBOFIX_SRC_MILITARY_SERVICE_SEED_MILITARY_SEED_PATHS_H_
#define KBOFIX_SRC_MILITARY_SERVICE_SEED_MILITARY_SEED_PATHS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_get_save_military_service_seed_path(char* out, size_t out_size);
int kbo_get_global_military_service_seed_path(char* out, size_t out_size);
int kbo_get_save_military_service_resolved_path(char* out, size_t out_size);
int kbo_get_current_players_dat_path_for_military_seed(char* out, size_t out_size);

#endif
