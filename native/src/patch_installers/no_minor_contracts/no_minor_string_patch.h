#ifndef KBOFIX_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_NO_MINOR_STRING_PATCH_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_NO_MINOR_STRING_PATCH_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int patch_kbo_no_minor_contract_string_at(uint8_t* target, const char* from, const char* to, size_t len);
int install_kbo_no_minor_contract_string_patch(HMODULE exe);

#endif
