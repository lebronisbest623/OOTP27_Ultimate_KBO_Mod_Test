#include "../internal/no_minor_experimental_patch_internal.h"

int install_kbo_no_minor_contract_experimental_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO no-minor-contract experimental patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO no-minor-contract experimental patch host=%s", host);
        return 0;
    }

    kbo_enable_no_minor_contract_demand_floor();

    int ok = 0;
    ok |= install_kbo_no_minor_contract_base_patches(exe);
    ok |= install_kbo_no_minor_contract_dynamic_flag_patches(exe);
    ok |= install_kbo_no_minor_contract_offer_ui_patches(exe);
    ok |= install_kbo_no_minor_contract_write_site_patches(exe);
    ok |= install_kbo_no_minor_contract_scan_patch(exe);
    ok |= install_kbo_no_minor_contract_string_patch(exe);

    append_logf("KBO no-minor-contract experimental patch complete installed_any=%d", ok);
    return ok;
}
