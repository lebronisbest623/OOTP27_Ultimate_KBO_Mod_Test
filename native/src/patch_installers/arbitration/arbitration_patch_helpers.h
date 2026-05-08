#ifndef KBOFIX_SRC_PATCH_INSTALLERS_ARBITRATION_ARBITRATION_PATCH_HELPERS_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_ARBITRATION_ARBITRATION_PATCH_HELPERS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* allocate_kbo_salary_arbitration_stub(const uint8_t* code, size_t size);

#endif
