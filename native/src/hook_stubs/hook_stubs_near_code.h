#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_NEAR_CODE_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_NEAR_CODE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* kbo_alloc_near_code(void* target, size_t size);

#endif
