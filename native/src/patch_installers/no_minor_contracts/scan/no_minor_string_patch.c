#include "no_minor_string_patch.h"
#include "../../../core/logging/core_log.h"

int patch_kbo_no_minor_contract_string_at(uint8_t* target, const char* from, const char* to, size_t len)
{
    (void)target;
    (void)from;
    (void)to;
    (void)len;
    append_log_line("KBO no-minor-contract string patch retired: refusing broad text/data mutation");
    return 0;
}

int install_kbo_no_minor_contract_string_patch(HMODULE exe)
{
    (void)exe;
    append_log_line("KBO no-minor-contract string patch retired: scoped offer UI patches handle labels");
    return 0;
}
