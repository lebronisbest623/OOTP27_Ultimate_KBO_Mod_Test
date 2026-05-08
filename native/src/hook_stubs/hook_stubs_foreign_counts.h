#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_FOREIGN_COUNTS_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_FOREIGN_COUNTS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_active_foreign_hitter_count_detour_stub(void* original_trampoline);
uint8_t* build_kbo_active_foreign_pitcher_count_detour_stub(void* original_trampoline);
uint8_t* build_kbo_callup_foreign_limit_branch_stub(void* allow_continuation, void* block_continuation, void* fail_continuation, void* wrapper, int total_check);

#endif
