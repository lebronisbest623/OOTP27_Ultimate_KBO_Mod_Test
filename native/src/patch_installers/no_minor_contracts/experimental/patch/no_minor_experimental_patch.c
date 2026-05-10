#include "../internal/no_minor_experimental_patch_internal.h"

int install_kbo_no_minor_contract_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO no-minor-contract patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO no-minor-contract patch host=%s", host);
        return 0;
    }

    kbo_enable_no_minor_contract_demand_floor();

    int ok = 0;
    ok |= install_kbo_no_minor_contract_base_patches(exe);
    ok |= install_kbo_no_minor_contract_offer_ui_patches(exe);
    append_log_line("KBO no-minor-contract dynamic +0x8a8 patches retired: unsafe outside scoped offer/submit flow");
    append_log_line("KBO no-minor-contract write-site patches retired: unsafe player/import/parser state writes");
    append_log_line("KBO no-minor-contract scan patch retired: unsafe global +0x8a8 writes can corrupt stock OOTP season/storyline state");
    append_log_line("KBO no-minor-contract global string patch retired: avoiding broad text/data mutation");

    append_logf("KBO no-minor-contract patch complete installed_any=%d", ok);
    return ok;
}

int install_kbo_no_minor_contract_experimental_patch(void)
{
    return install_kbo_no_minor_contract_patch();
}
