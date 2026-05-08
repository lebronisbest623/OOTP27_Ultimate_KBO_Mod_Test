#ifndef KBOFIX_SRC_HOOK_STUBS_FOREIGN_SIGNABILITY_STUBS_SIGNING_BRANCH_STUBS_H_
#define KBOFIX_SRC_HOOK_STUBS_FOREIGN_SIGNABILITY_STUBS_SIGNING_BRANCH_STUBS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_fa_signing_branch_detour_stub(void* original_trampoline);
uint8_t* build_kbo_fa_signing_success_post_stub(void* continuation, void* patch_site);

#endif
