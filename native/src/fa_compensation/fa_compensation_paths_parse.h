#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_PATHS_PARSE_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_PATHS_PARSE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_get_fa_compensation_path(char* out, size_t out_size);
int kbo_get_fa_compensation_protected_lists_path(char* out, size_t out_size);
int kbo_get_fa_compensation_decisions_path(char* out, size_t out_size);
int kbo_get_fa_compensation_protection_debug_path(char* out, size_t out_size);
uint32_t kbo_fa_compensation_parse_u32(const char* text);
int32_t kbo_fa_compensation_parse_i32(const char* text);
void kbo_fa_compensation_copy_token(const char* value, char* out, size_t out_size);

#endif
