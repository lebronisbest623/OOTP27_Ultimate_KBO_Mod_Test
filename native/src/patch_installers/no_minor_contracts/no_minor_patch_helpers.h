#ifndef KBOFIX_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_NO_MINOR_PATCH_HELPERS_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_NO_MINOR_PATCH_HELPERS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int patch_kbo_no_minor_contract_write_site_by_masked_context(HMODULE exe, const char* label, uint32_t rva, const uint8_t* expected, const uint8_t* patch, size_t size, const uint8_t* context, const uint8_t* context_mask, size_t context_size, size_t target_offset);
int patch_kbo_no_minor_contract_write_site_by_pattern(HMODULE exe, const char* label, uint32_t rva, const uint8_t* expected, const uint8_t* patch, size_t size);
int patch_kbo_no_minor_contract_write_site_by_pattern_ordinal(HMODULE exe, const char* label, const uint8_t* expected, const uint8_t* patch, size_t size, int desired_index, int expected_hits);

#endif
