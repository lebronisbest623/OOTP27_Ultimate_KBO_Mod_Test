#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_PATHS_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_PATHS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int get_kbo_foreign_waiver_event_path(char* out, size_t out_size);
int kbo_get_foreign_waiver_rights_path(char* out, size_t out_size);
int get_kbo_foreign_waiver_decisions_path(char* out, size_t out_size);
int get_kbo_asian_quota_nation_ids_path(char* out, size_t out_size);
int get_kbo_foreign_waiver_announcement_path(char* out, size_t out_size);

#endif
