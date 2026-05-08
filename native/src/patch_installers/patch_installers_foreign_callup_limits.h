#ifndef KBOFIX_SRC_PATCH_INSTALLERS_PATCH_INSTALLERS_FOREIGN_CALLUP_LIMITS_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_PATCH_INSTALLERS_FOREIGN_CALLUP_LIMITS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int install_kbo_callup_foreign_limit_branch_patch(const char* label, uint32_t target_rva, int32_t allow_delta, int32_t fallback_delta, const uint8_t* expected, size_t patch_len, void* wrapper, int total_check);
int install_kbo_callup_foreign_limit_branch_patches(void);

#endif
