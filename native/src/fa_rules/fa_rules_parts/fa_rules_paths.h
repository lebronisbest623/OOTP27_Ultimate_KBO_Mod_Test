#ifndef KBOFIX_SRC_FA_RULES_FA_RULES_PARTS_FA_RULES_PATHS_H_
#define KBOFIX_SRC_FA_RULES_FA_RULES_PARTS_FA_RULES_PATHS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_fa_rules_get_localappdata_file_path(const char* file_name, char* out, size_t out_size);
int kbo_fa_rules_resolve_existing_path(char* out, size_t out_size);

#endif
