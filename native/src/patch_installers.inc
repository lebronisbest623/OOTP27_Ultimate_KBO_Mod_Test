static int install_kbo_military_service_entry_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO military service entry patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO military service entry patch host=%s", host);
        return 0;
    }

    const uint8_t expected_signature[] = {
        0x40, 0x55,                                     // push rbp
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x41, 0x56,                                     // push r14
        0x41, 0x57,                                     // push r15
        0x48, 0x8D, 0x6C, 0x24, 0xE1                    // lea rbp, [rsp - 0x1f]
    };
    uint8_t* target = find_ootp_executable_pattern(expected_signature, sizeof(expected_signature));
    if (target == NULL) {
        append_log_line("Could not resolve military service entry function by signature");
        return 0;
    }
    const size_t stolen_len = 18;
    uint8_t expected[18] = {
        0x40, 0x55,                                     // push rbp
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x41, 0x56,                                     // push r14
        0x41, 0x57,                                     // push r15
        0x48, 0x8D, 0x6C, 0x24, 0xE1                    // lea rbp, [rsp - 0x1f]
    };

    if (target[0] == 0x48 && target[1] == 0xB8 && target[10] == 0xFF && target[11] == 0xE0) {
        append_logf("KBO military service entry patch already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO military service entry patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO military service entry trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_military_service_entry_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO military service entry detour stub");
        return 0;
    }

    uint8_t patch[18] = {
        0x48, 0xB8,                                     // mov rax, stub
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO military service entry patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO military service entry patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_military_service_entry_wrapper);
    return 1;
}

static int install_kbo_military_status_update_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO military status update patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO military status update patch host=%s", host);
        return 0;
    }

    const uint8_t expected_signature[] = {
        0x48, 0x89, 0x4C, 0x24, 0x08,                   // mov [rsp + 8], rcx
        0x55,                                           // push rbp
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x41, 0x56,                                     // push r14
        0x41, 0x57                                      // push r15
    };
    uint8_t* target = find_ootp_executable_pattern(expected_signature, sizeof(expected_signature));
    if (target == NULL) {
        append_log_line("Could not resolve military status update function by signature");
        return 0;
    }
    const size_t stolen_len = 17;
    uint8_t expected[17] = {
        0x48, 0x89, 0x4C, 0x24, 0x08,                   // mov [rsp + 8], rcx
        0x55,                                           // push rbp
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x41, 0x56,                                     // push r14
        0x41, 0x57                                      // push r15
    };

    if (target[0] == 0x48 && target[1] == 0xB8 && target[10] == 0xFF && target[11] == 0xE0) {
        append_logf("KBO military status update patch already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO military status update patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO military status update trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_military_status_update_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO military status update detour stub");
        return 0;
    }

    uint8_t patch[17] = {
        0x48, 0xB8,                                     // mov rax, stub
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO military status update patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO military status update patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_military_status_update_wrapper);
    return 1;
}

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

static int install_kbo_ai_fa_signability_random_fallback_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI FA signability fallback patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI FA signability fallback patch host=%s", host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_AI_FA_SIGNABILITY_RANDOM_FALLBACK_RVA);
    const uint8_t expected[60] = {
        0x85, 0xC0,                                           /* test eax,eax */
        0x75, 0x38,                                           /* jne +0x38 */
        0x8B, 0x8B, 0xB8, 0x0F, 0x00, 0x00,                   /* mov ecx,[rbx+0xfb8] */
        0x48, 0x8B, 0x85, 0x40, 0x01, 0x00, 0x00,             /* mov rax,[rbp+0x140] */
        0x03, 0x88, 0x50, 0x44, 0x00, 0x00,                   /* add ecx,[rax+0x4450] */
        0xB8, 0xE9, 0xA2, 0x8B, 0x2E,                         /* mov eax,0x2e8ba2e9 */
        0xF7, 0xE9,                                           /* imul ecx */
        0xD1, 0xFA,                                           /* sar edx,1 */
        0x8B, 0xC2,                                           /* mov eax,edx */
        0xC1, 0xE8, 0x1F,                                     /* shr eax,0x1f */
        0x03, 0xD0,                                           /* add edx,eax */
        0x6B, 0xC2, 0x0B,                                     /* imul eax,edx,0xb */
        0x2B, 0xC8,                                           /* sub ecx,eax */
        0x45, 0x0F, 0xB6, 0xF6,                               /* movzx r14d,r14b */
        0x83, 0xF9, 0x01,                                     /* cmp ecx,1 */
        0xB8, 0x00, 0x00, 0x00, 0x00,                         /* mov eax,0 */
        0x44, 0x0F, 0x45, 0xF0                                /* cmovne r14d,eax */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO AI FA signability fallback patch target unreadable target=%p", target);
        return 0;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO AI FA signability fallback patch", target, sizeof(expected));
        return 0;
    }

    uint8_t patch[60];
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0x85;                                         /* test eax,eax */
    patch[1] = 0xC0;
    patch[2] = 0x0F;                                         /* je 0x140af1fe9 */
    patch[3] = 0x84;
    patch[4] = 0x5E;
    patch[5] = 0x02;
    patch[6] = 0x00;
    patch[7] = 0x00;
    patch[8] = 0xE9;                                         /* jmp 0x140af1dbf */
    patch[9] = 0x2F;
    patch[10] = 0x00;
    patch[11] = 0x00;
    patch[12] = 0x00;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO AI FA signability fallback patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf("installed KBO AI FA signability fallback patch target=%p", target);
    return 1;
}
