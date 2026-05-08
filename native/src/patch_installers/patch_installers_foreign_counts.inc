static int install_kbo_active_foreign_count_patch(
    const char* label,
    uint32_t target_rva,
    const uint8_t* context,
    size_t context_size,
    size_t target_offset,
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

    const size_t stolen_len = 20;
    const uint8_t expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x6C, 0x24, 0x10,
        0x48, 0x89, 0x74, 0x24, 0x18,
        0x57,
        0x48, 0x83, 0xEC, 0x20
    };

    uint8_t* target = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        target_rva,
        expected,
        sizeof(expected),
        context,
        context_size,
        target_offset,
        label);
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
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
    const uint8_t context[48] = {
        0x40, 0x41, 0x0F, 0xBF, 0xC3, 0x48, 0x83, 0xC4,
        0x20, 0x5F, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
        0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xE9, 0x48,
        0x8D, 0x99, 0x80, 0x2A, 0x00, 0x00, 0x33, 0xFF
    };
    return install_kbo_active_foreign_count_patch(
        "KBO active foreign hitter count patch",
        OOTP27_ACTIVE_FOREIGN_HITTER_COUNT_RVA,
        context,
        sizeof(context),
        16u,
        build_kbo_active_foreign_hitter_count_detour_stub);
}

static int install_kbo_active_foreign_pitcher_count_patch(void)
{
    const uint8_t context[48] = {
        0x20, 0x5F, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
        0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xE9, 0x48,
        0x8D, 0x99, 0x80, 0x2A, 0x00, 0x00, 0x33, 0xFF
    };
    return install_kbo_active_foreign_count_patch(
        "KBO active foreign pitcher count patch",
        OOTP27_ACTIVE_FOREIGN_PITCHER_COUNT_RVA,
        context,
        sizeof(context),
        16u,
        build_kbo_active_foreign_pitcher_count_detour_stub);
}

static int install_kbo_secondary_foreign_hitter_count_patch(void)
{
    const uint8_t context[64] = {
        0x24, 0x40, 0x0F, 0xBF, 0xC7, 0x48, 0x83, 0xC4,
        0x20, 0x5F, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
        0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xE9, 0x48,
        0x8D, 0x99, 0xE0, 0x33, 0x00, 0x00, 0x33, 0xFF,
        0xBE, 0xC8, 0x00, 0x00, 0x00, 0x8B, 0x13, 0x85
    };
    return install_kbo_active_foreign_count_patch(
        "KBO secondary foreign hitter count patch",
        OOTP27_SECONDARY_FOREIGN_HITTER_COUNT_RVA,
        context,
        sizeof(context),
        24u,
        build_kbo_active_foreign_hitter_count_detour_stub);
}

static int install_kbo_secondary_foreign_pitcher_count_patch(void)
{
    const uint8_t context[64] = {
        0x24, 0x40, 0x0F, 0xBF, 0xC7, 0x48, 0x83, 0xC4,
        0x20, 0x5F, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
        0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xE9, 0x48,
        0x8D, 0x99, 0xE0, 0x33, 0x00, 0x00, 0x33, 0xFF,
        0xBE, 0xC8, 0x00, 0x00, 0x00, 0x83, 0xBB, 0xA0
    };
    return install_kbo_active_foreign_count_patch(
        "KBO secondary foreign pitcher count patch",
        OOTP27_SECONDARY_FOREIGN_PITCHER_COUNT_RVA,
        context,
        sizeof(context),
        24u,
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
