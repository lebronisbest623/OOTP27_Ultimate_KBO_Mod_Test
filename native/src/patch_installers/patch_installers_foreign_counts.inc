static int install_kbo_active_foreign_count_patch(
    const char* label,
    uint32_t target_rva,
    uint8_t* (*build_stub)(void*))
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_logf("GetModuleHandleA(NULL) failed for %s", label);
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping %s host=%s", label, host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + target_rva);
    const size_t stolen_len = 20;
    const uint8_t expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x6C, 0x24, 0x10,
        0x48, 0x89, 0x74, 0x24, 0x18,
        0x57,
        0x48, 0x83, 0xEC, 0x20
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("%s target unreadable target=%p", label, target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch(label, target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_logf("failed to allocate %s trampoline", label);
        return 0;
    }

    uint8_t* stub = build_stub(trampoline);
    if (stub == NULL) {
        append_logf("failed to allocate %s detour stub", label);
        return 0;
    }

    uint8_t patch[20] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf("installed %s target=%p stub=%p trampoline=%p", label, target, stub, trampoline);
    return 1;
}

static int install_kbo_active_foreign_hitter_count_patch(void)
{
    return install_kbo_active_foreign_count_patch(
        "KBO active foreign hitter count patch",
        OOTP27_ACTIVE_FOREIGN_HITTER_COUNT_RVA,
        build_kbo_active_foreign_hitter_count_detour_stub);
}

static int install_kbo_active_foreign_pitcher_count_patch(void)
{
    return install_kbo_active_foreign_count_patch(
        "KBO active foreign pitcher count patch",
        OOTP27_ACTIVE_FOREIGN_PITCHER_COUNT_RVA,
        build_kbo_active_foreign_pitcher_count_detour_stub);
}

static int install_kbo_secondary_foreign_hitter_count_patch(void)
{
    return install_kbo_active_foreign_count_patch(
        "KBO secondary foreign hitter count patch",
        OOTP27_SECONDARY_FOREIGN_HITTER_COUNT_RVA,
        build_kbo_active_foreign_hitter_count_detour_stub);
}

static int install_kbo_secondary_foreign_pitcher_count_patch(void)
{
    return install_kbo_active_foreign_count_patch(
        "KBO secondary foreign pitcher count patch",
        OOTP27_SECONDARY_FOREIGN_PITCHER_COUNT_RVA,
        build_kbo_active_foreign_pitcher_count_detour_stub);
}

static int install_kbo_foreign_count_patches(void)
{
    int ok = 1;
    ok &= install_kbo_active_foreign_hitter_count_patch();
    ok &= install_kbo_active_foreign_pitcher_count_patch();
    ok &= install_kbo_secondary_foreign_hitter_count_patch();
    ok &= install_kbo_secondary_foreign_pitcher_count_patch();
    return ok;
}

