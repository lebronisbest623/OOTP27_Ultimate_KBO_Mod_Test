#ifndef KBOFIX_SRC_HOOK_STUBS_COMPETITIVE_BALANCE_TAX_HOOK_STUBS_CBT_DRAFT_ORDER_H_
#define KBOFIX_SRC_HOOK_STUBS_COMPETITIVE_BALANCE_TAX_HOOK_STUBS_CBT_DRAFT_ORDER_H_

#include <stddef.h>
#include <stdint.h>

uint8_t* build_kbo_cbt_draft_order_trampoline(void* original_address, size_t stolen_len);
uint8_t* build_kbo_cbt_draft_order_detour_stub(void* original_trampoline);

#endif
