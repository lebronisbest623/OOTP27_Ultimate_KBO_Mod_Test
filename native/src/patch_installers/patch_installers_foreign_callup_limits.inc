static int install_kbo_callup_foreign_limit_branch_patch(
    const char* label,
    uint32_t target_rva,
    uint32_t allow_rva,
    const uint8_t* expected,
    size_t patch_len,
    void* wrapper,
    int total_check)
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
    if (!memory_range_readable(target, patch_len)) {
        append_logf("%s target unreadable target=%p", label, target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, patch_len) != 0) {
        log_patch_bytes_mismatch(label, target, patch_len);
        return 0;
    }

    uint8_t* stub = build_kbo_callup_foreign_limit_branch_stub(
        (uint8_t*)((uintptr_t)exe + allow_rva),
        target + patch_len,
        (uint8_t*)((uintptr_t)exe + 0x0107B2B4u),
        wrapper,
        total_check);
    if (stub == NULL) {
        append_logf("failed to allocate %s stub", label);
        return 0;
    }

    uint8_t patch[32] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90
    };
    if (patch_len < 13 || patch_len > sizeof(patch)) {
        append_logf("%s invalid patch_len=%llu", label, (unsigned long long)patch_len);
        return 0;
    }
    for (size_t i = 13; i < patch_len; i++) {
        patch[i] = 0x90;
    }
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }
    memcpy(target, patch, patch_len);
    FlushInstructionCache(GetCurrentProcess(), target, patch_len);
    DWORD ignored = 0;
    VirtualProtect(target, patch_len, old_protect, &ignored);

    append_logf("installed %s target=%p stub=%p patch_len=%llu", label, target, stub, (unsigned long long)patch_len);
    return 1;
}

static int install_kbo_callup_foreign_limit_branch_patches(void)
{
    const uint8_t hitter_expected[17] = {
        0x3B, 0xC2, 0x0F, 0x8C, 0x8B, 0x00, 0x00, 0x00,
        0x45, 0x84, 0xE4, 0x0F, 0x85, 0x1E, 0x18, 0x00,
        0x00
    };
    const uint8_t pitcher_expected[13] = {
        0x3B, 0xC2, 0x7C, 0x52, 0x45, 0x84, 0xE4,
        0x0F, 0x85, 0x35, 0x17, 0x00, 0x00
    };
    const uint8_t total_expected[13] = {
        0x3B, 0xD1, 0x7C, 0x52, 0x45, 0x84, 0xE4,
        0x0F, 0x85, 0x85, 0x16, 0x00, 0x00
    };

    int ok = 1;
    ok &= install_kbo_callup_foreign_limit_branch_patch(
        "KBO callup foreign hitter limit branch patch",
        OOTP27_CALLUP_FOREIGN_HITTER_LIMIT_BRANCH_RVA,
        0x01079B18u,
        hitter_expected,
        sizeof(hitter_expected),
        &ootp_kbo_callup_foreign_hitter_limit_allows_wrapper,
        0);
    ok &= install_kbo_callup_foreign_limit_branch_patch(
        "KBO callup foreign pitcher limit branch patch",
        OOTP27_CALLUP_FOREIGN_PITCHER_LIMIT_BRANCH_RVA,
        0x01079BC8u,
        pitcher_expected,
        sizeof(pitcher_expected),
        &ootp_kbo_callup_foreign_pitcher_limit_allows_wrapper,
        0);
    ok &= install_kbo_callup_foreign_limit_branch_patch(
        "KBO callup foreign total limit branch patch",
        OOTP27_CALLUP_FOREIGN_TOTAL_LIMIT_BRANCH_RVA,
        0x01079C78u,
        total_expected,
        sizeof(total_expected),
        &ootp_kbo_callup_foreign_total_limit_allows_wrapper,
        1);
    return ok;
}

