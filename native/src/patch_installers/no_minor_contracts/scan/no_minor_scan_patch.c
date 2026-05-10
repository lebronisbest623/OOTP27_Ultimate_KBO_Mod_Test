#include "no_minor_scan_patch.h"
#include "../../../core/logging/core_log.h"

int patch_kbo_no_minor_contract_scan_byte(uint8_t* imm_ptr)
{
    (void)imm_ptr;
    append_log_line("KBO no-minor-contract scan byte patch retired: refusing broad +0x8a8 mutation");
    return 0;
}

int install_kbo_no_minor_contract_scan_patch(HMODULE exe)
{
    (void)exe;
    append_log_line("KBO no-minor-contract scan patch retired: refusing executable-wide +0x8a8 scan");
    return 0;
}
