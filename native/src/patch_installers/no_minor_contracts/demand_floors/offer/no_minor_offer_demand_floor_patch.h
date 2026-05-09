#ifndef KBOFIX_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_NO_MINOR_OFFER_DEMAND_FLOOR_PATCH_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_NO_MINOR_OFFER_DEMAND_FLOOR_PATCH_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int install_kbo_no_minor_contract_offer_player_demand_floor_patch(HMODULE exe, const char* label, uint32_t rva, const uint8_t* expected, size_t stolen_len, int alt_path);

#endif
