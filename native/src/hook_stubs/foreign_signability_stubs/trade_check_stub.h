#ifndef KBOFIX_SRC_HOOK_STUBS_FOREIGN_SIGNABILITY_STUBS_TRADE_CHECK_STUB_H_
#define KBOFIX_SRC_HOOK_STUBS_FOREIGN_SIGNABILITY_STUBS_TRADE_CHECK_STUB_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_trade_check_detour_stub(void* original_trampoline);

#endif
