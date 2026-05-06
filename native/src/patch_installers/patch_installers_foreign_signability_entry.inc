static int install_kbo_player_team_signability_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO player/team signability patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO player/team signability patch host=%s", host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_PLAYER_TEAM_SIGNABILITY_RVA);
    const size_t stolen_len = 16;
    const uint8_t expected[16] = {
        0x66, 0x44, 0x89, 0x44, 0x24, 0x18,             // mov [rsp + 0x18], r8w
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x48, 0x83, 0xEC, 0x28                          // sub rsp, 0x28
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO player/team signability patch target unreadable target=%p", target);
        return 0;
    }

    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO player/team signability patch already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO player/team signability patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO player/team signability trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_player_team_signability_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO player/team signability detour stub");
        return 0;
    }

    uint8_t patch[16] = {
        0x48, 0xB8,                                     // mov rax, stub
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO player/team signability patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO player/team signability patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_player_team_signability_wrapper);
    return 1;
}

static int install_kbo_player_offer_eligibility_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO player offer eligibility patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO player offer eligibility patch host=%s", host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_PLAYER_SIGNABILITY_BLOCK_849320_RVA);
    const size_t stolen_len = 20;
    const uint8_t expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,                   // mov [rsp + 0x08], rbx
        0x48, 0x89, 0x6C, 0x24, 0x10,                   // mov [rsp + 0x10], rbp
        0x48, 0x89, 0x74, 0x24, 0x18,                   // mov [rsp + 0x18], rsi
        0x48, 0x89, 0x7C, 0x24, 0x20                    // mov [rsp + 0x20], rdi
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO player offer eligibility patch target unreadable target=%p", target);
        return 0;
    }

    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO player offer eligibility patch already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO player offer eligibility patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO player offer eligibility trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_player_offer_eligibility_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO player offer eligibility detour stub");
        return 0;
    }

    uint8_t patch[20] = {
        0x48, 0xB8,                                     // mov rax, stub
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO player offer eligibility patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO player offer eligibility patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_player_offer_eligibility_wrapper);
    return 1;
}

