#ifndef KBOFIX_SRC_PATCH_INSTALLERS_PATCH_INSTALLERS_ALLSTAR_COMMON_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_PATCH_INSTALLERS_ALLSTAR_COMMON_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

HMODULE kbo_allstar_get_host_exe(const char* label);
int kbo_patch_static_pattern(const char* label, const uint8_t* pattern, size_t pattern_size, size_t patch_offset, const uint8_t* expected, const uint8_t* patch, size_t patch_size);

#endif
