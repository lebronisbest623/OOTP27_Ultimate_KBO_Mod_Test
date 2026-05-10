#include "../internal/no_minor_experimental_patch_internal.h"

int install_kbo_no_minor_contract_write_site_patches(HMODULE exe)
{
    (void)exe;
    append_log_line("KBO no-minor-contract write-site patches retired: broad player +0x8a8 writes are unsafe");
    return 0;
}
