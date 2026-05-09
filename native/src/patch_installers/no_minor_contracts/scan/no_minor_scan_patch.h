#ifndef KBOFIX_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_NO_MINOR_SCAN_PATCH_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_NO_MINOR_SCAN_PATCH_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int patch_kbo_no_minor_contract_scan_byte(uint8_t* imm_ptr);
int install_kbo_no_minor_contract_scan_patch(HMODULE exe);

#endif
