#ifndef KBOFIX_SRC_PATCH_INSTALLERS_PATCH_INSTALLERS_FOREIGN_COUNTS_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_PATCH_INSTALLERS_FOREIGN_COUNTS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int install_kbo_active_foreign_count_patch(const char* label, uint32_t target_rva, const uint8_t* context, size_t context_size, size_t target_offset, uint8_t* (*build_stub)(void*));
int install_kbo_active_foreign_hitter_count_patch(void);
int install_kbo_active_foreign_pitcher_count_patch(void);
int install_kbo_secondary_foreign_hitter_count_patch(void);
int install_kbo_secondary_foreign_pitcher_count_patch(void);
int install_kbo_foreign_count_patches(void);

#endif
