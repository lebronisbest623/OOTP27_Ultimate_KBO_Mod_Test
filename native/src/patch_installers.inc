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

static int install_kbo_fa_submit_offer_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO FA submit-offer probe patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO FA submit-offer probe patch host=%s", host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_FA_SUBMIT_OFFER_PROBE_RVA);
    const size_t stolen_len = 21;
    const uint8_t expected[21] = {
        0x48, 0x89, 0x5C, 0x24, 0x20,                   /* mov [rsp+0x20],rbx */
        0x48, 0x89, 0x4C, 0x24, 0x08,                   /* mov [rsp+0x8],rcx */
        0x55,                                           /* push rbp */
        0x56,                                           /* push rsi */
        0x57,                                           /* push rdi */
        0x41, 0x54,                                     /* push r12 */
        0x41, 0x55,                                     /* push r13 */
        0x41, 0x56,                                     /* push r14 */
        0x41, 0x57                                      /* push r15 */
    };

    if (!memory_range_readable(target, stolen_len)) {
        append_logf("KBO FA submit-offer probe patch target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO FA submit-offer probe patch already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO FA submit-offer probe patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO FA submit-offer probe trampoline");
        return 0;
    }
    uint8_t* stub = build_kbo_fa_submit_offer_probe_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO FA submit-offer probe detour stub");
        return 0;
    }

    uint8_t patch[21] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO FA submit-offer probe patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO FA submit-offer probe patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_fa_submit_offer_probe_wrapper);
    return 1;
}

static int kbo_fa_signing_branch_has_news_literal(uint8_t* signature_start)
{
    static const char literal[] = "Signed free agent %p.";
    if (signature_start == NULL) {
        return 0;
    }

    for (size_t off = 0; off < 0x140u; off++) {
        uint8_t* insn = signature_start + off;
        if (!memory_range_readable(insn, 7u)) {
            continue;
        }
        if (insn[0] != 0x4c || insn[1] != 0x8d || insn[2] != 0x05) {
            continue;
        }

        int32_t rel = 0;
        memcpy(&rel, insn + 3, sizeof(rel));
        const char* target = (const char*)(insn + 7 + rel);
        if (memory_range_readable(target, sizeof(literal)) && memcmp(target, literal, sizeof(literal)) == 0) {
            return 1;
        }
    }

    return 0;
}

static int kbo_matches_fa_signing_branch_signature(uint8_t* candidate)
{
    static const uint8_t pattern[] = {
        0x41, 0x8B, 0x57, 0x58,                         /* mov edx,[r15+0x58] */
        0x85, 0xD2,                                     /* test edx,edx */
        0x74, 0x0C,                                     /* je +0x0c */
        0x49, 0x8B, 0xCF,                               /* mov rcx,r15 */
        0xE8, 0x00, 0x00, 0x00, 0x00,                   /* call player team cleanup */
        0x41, 0x8B, 0x57, 0x58,                         /* mov edx,[r15+0x58] */
        0x45, 0x33, 0xF6,                               /* xor r14d,r14d */
        0x41, 0x89, 0x57, 0x5C,
        0x45, 0x89, 0x77, 0x58,
        0x41, 0x8B, 0x47, 0x60,
        0x41, 0x89, 0x47, 0x64,
        0x45, 0x89, 0x77, 0x60,
        0x41, 0x8B, 0x47, 0x48,
        0x41, 0x89, 0x47, 0x4C,
        0x45, 0x89, 0x77, 0x48,
        0x66, 0x41, 0xC7, 0x47, 0x41, 0x01, 0x01,
        0x4D, 0x85, 0xE4,
        0x0F, 0x84
    };

    if (candidate == NULL) {
        return 0;
    }

    for (size_t i = 0; i < sizeof(pattern); i++) {
        if (i >= 12u && i <= 15u) {
            continue;
        }
        if (candidate[i] != pattern[i]) {
            return 0;
        }
    }

    return kbo_fa_signing_branch_has_news_literal(candidate);
}

static uint8_t* resolve_kbo_fa_signing_branch_target(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return NULL;
    }

    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;

    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < 96u) {
            continue;
        }

        DWORD scan_limit = vsz - 96u;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (kbo_matches_fa_signing_branch_signature(candidate)) {
                return candidate + OOTP27_FA_SIGNING_BRANCH_PATCH_OFFSET;
            }
        }
    }

    return NULL;
}

static int install_kbo_fa_signing_branch_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO FA signing branch patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO FA signing branch patch host=%s", host);
        return 0;
    }

    uint8_t* target = resolve_kbo_fa_signing_branch_target();
    const size_t stolen_len = 16;
    const uint8_t expected[16] = {
        0x45, 0x89, 0x77, 0x60,                         /* mov [r15+0x60],r14d */
        0x41, 0x8B, 0x47, 0x48,                         /* mov eax,[r15+0x48] */
        0x41, 0x89, 0x47, 0x4C,                         /* mov [r15+0x4c],eax */
        0x45, 0x89, 0x77, 0x48                          /* mov [r15+0x48],r14d */
    };

    if (target == NULL) {
        append_log_line("Could not resolve KBO FA signing branch by signature");
        return 0;
    }
    if (!memory_range_readable(target, stolen_len)) {
        append_logf("KBO FA signing branch patch target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO FA signing branch patch already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO FA signing branch patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO FA signing branch trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_fa_signing_branch_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO FA signing branch detour stub");
        return 0;
    }

    uint8_t patch[16] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO FA signing branch patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO FA signing branch patch target=%p rva=0x%llx stub=%p trampoline=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        trampoline,
        &ootp_kbo_fa_signing_branch_wrapper);
    return 1;
}

static int install_kbo_ai_fa_status_candidate_insert_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI FA status candidate insert patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI FA status candidate insert patch host=%s", host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_AI_FA_STATUS_CANDIDATE_INSERT_RVA);
    const uint8_t expected[12] = {
        0x48, 0x63, 0xC1,                               /* movsxd rax,ecx */
        0x4D, 0x89, 0x3C, 0xC6,                         /* mov [r14+rax*8],r15 */
        0xFF, 0xC1,                                     /* inc ecx */
        0x89, 0x4D, 0xD4                                /* mov [rbp-0x2c],ecx */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO AI FA status candidate insert patch target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO AI FA status candidate insert patch already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO AI FA status candidate insert patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_ai_fa_status_candidate_insert_stub(target + sizeof(expected));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO AI FA status candidate insert stub");
        return 0;
    }

    uint8_t patch[12] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      /* jmp rax */
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO AI FA status candidate insert patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO AI FA status candidate insert patch target=%p stub=%p wrapper=%p",
        target,
        stub,
        &ootp_kbo_ai_fa_status_candidate_insert_wrapper);
    return 1;
}

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

static HMODULE kbo_allstar_get_host_exe(const char* label)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_logf("GetModuleHandleA(NULL) failed for %s", label);
        return NULL;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping %s host=%s", label, host);
        return NULL;
    }

    if (!kbo_fix_enabled()) {
        append_logf("%s skipped: opt-in disabled", label);
        return NULL;
    }

    return exe;
}

static int kbo_patch_static_pattern(
    const char* label,
    const uint8_t* pattern,
    size_t pattern_size,
    size_t patch_offset,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t patch_size)
{
    uint8_t* base = find_ootp_executable_pattern(pattern, pattern_size);
    if (base == NULL) {
        return 0;
    }
    return patch_static_bytes(label, base + patch_offset, expected, patch, patch_size);
}

static int install_single_division_allstar_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO single-division all-star patch");
    if (exe == NULL) {
        return 0;
    }

    int ok = 0;

    const uint8_t prep_context_may[] = {
        0xF6, 0x42, 0x4C, 0x01,
        0x0F, 0x84, 0xC0, 0x00, 0x00, 0x00
    };
    const uint8_t prep_expected[6] = {
        0x0F, 0x84, 0xC0, 0x00, 0x00, 0x00
    };
    const uint8_t prep_patch[6] = {
        0xE9, 0xC1, 0x00, 0x00, 0x00, 0x90
    };
    ok |= kbo_patch_static_pattern(
        "KBO single-division all-star prep gate",
        prep_context_may,
        sizeof(prep_context_may),
        4,
        prep_expected,
        prep_patch,
        sizeof(prep_expected));
    if (!ok) {
        uint8_t* prep_gate = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_PREP_SINGLE_DIVISION_GATE_RVA);
        if (memory_range_readable(prep_gate, sizeof(prep_expected))) {
            ok |= patch_static_bytes(
                "KBO single-division all-star prep gate",
                prep_gate,
                prep_expected,
                prep_patch,
                sizeof(prep_expected));
        }
    }

    const uint8_t roster_context_may[] = {
        0x41, 0x8B, 0x46, 0x4C,
        0xA8, 0x01,
        0x74, 0x0D
    };
    const uint8_t roster_expected[2] = {0x74, 0x0D};
    const uint8_t roster_patch[2] = {0xEB, 0x0D};
    int roster_ok = kbo_patch_static_pattern(
        "KBO single-division all-star roster gate",
        roster_context_may,
        sizeof(roster_context_may),
        6,
        roster_expected,
        roster_patch,
        sizeof(roster_expected));
    if (!roster_ok) {
        uint8_t* roster_gate = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_ROSTER_SINGLE_DIVISION_GATE_RVA);
        if (memory_range_readable(roster_gate, sizeof(roster_expected))) {
            roster_ok = patch_static_bytes(
                "KBO single-division all-star roster gate",
                roster_gate,
                roster_expected,
                roster_patch,
                sizeof(roster_expected));
        }
    }
    ok |= roster_ok;

    const uint8_t schedule_context_may[] = {
        0x41, 0x80, 0xBD, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x7F,
        0x41, 0xC6, 0x85, 0xF0, 0x45, 0x00, 0x00, 0x01,
        0x41, 0x0F, 0xB6, 0x85, 0x22, 0x01, 0x00, 0x00
    };
    const uint8_t schedule_expected[2] = {0x74, 0x7F};
    const uint8_t schedule_patch[2] = {0x90, 0x90};
    int schedule_ok = kbo_patch_static_pattern(
        "KBO all-star schedule day import gate",
        schedule_context_may,
        sizeof(schedule_context_may),
        8,
        schedule_expected,
        schedule_patch,
        sizeof(schedule_expected));
    if (!schedule_ok) {
        uint8_t* schedule_day_gate = (uint8_t*)((uintptr_t)exe + OOTP27_IMPORT_SCHEDULE_ALLSTAR_DAY_GATE_RVA);
        if (memory_range_readable(schedule_day_gate, sizeof(schedule_expected))) {
            schedule_ok = patch_static_bytes(
                "KBO all-star schedule day import gate",
                schedule_day_gate,
                schedule_expected,
                schedule_patch,
                sizeof(schedule_expected));
        } else {
            append_log_line("KBO all-star schedule day import gate skipped: fallback RVA unreadable");
        }
    } else {
        ok |= schedule_ok;
    }
    ok |= schedule_ok;

    return ok;
}

static int install_allstar_team_setup_single_division_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star team setup single-division patch");
    if (exe == NULL) {
        return 0;
    }

    const uint8_t context_may[] = {
        0xF6, 0x41, 0x4C, 0x01,
        0x0F, 0x85, 0x74, 0x02, 0x00, 0x00
    };
    const uint8_t expected[6] = {
        0x0F, 0x85, 0x74, 0x02, 0x00, 0x00
    };
    const uint8_t patch[6] = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    int ok = kbo_patch_static_pattern(
        "KBO all-star team setup single-division bail gate",
        context_may,
        sizeof(context_may),
        4,
        expected,
        patch,
        sizeof(expected));
    if (ok) {
        return 1;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_TEAM_SETUP_SINGLE_DIVISION_BAIL_RVA);
    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO all-star team setup single-division bail gate target unreadable target=%p", target);
        return 0;
    }

    return patch_static_bytes(
        "KBO all-star team setup single-division bail gate",
        target,
        expected,
        patch,
        sizeof(expected));
}

static int install_allstar_candidate_team_split_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star candidate team split patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[27] = {
        0x45, 0x39, 0xAE, 0x44, 0x4A, 0x00, 0x00,
        0x7E, 0x0C,
        0x49, 0x8B, 0x86, 0x38, 0x4A, 0x00, 0x00,
        0x4C, 0x8B, 0x30,
        0xEB, 0x03,
        0x4D, 0x8B, 0xF5,
        0x45, 0x8B, 0xFD
    };
    const uint8_t april_expected[27] = {
        0x45, 0x39, 0xAE, 0x3C, 0x4A, 0x00, 0x00,
        0x7E, 0x0C,
        0x49, 0x8B, 0x86, 0x30, 0x4A, 0x00, 0x00,
        0x4C, 0x8B, 0x30,
        0xEB, 0x03,
        0x4D, 0x8B, 0xF5,
        0x45, 0x8B, 0xFD
    };

    uint8_t* target = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    const uint8_t* expected = may_expected;
    if (target == NULL) {
        target = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }
    if (target == NULL) {
        target = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_SITE_RVA);
        expected = april_expected;
    }

    if (!memory_range_readable(target, sizeof(april_expected))) {
        append_logf("KBO all-star candidate team split hook target unreadable target=%p", target);
        return 0;
    }

    int ok = 0;
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        append_logf("KBO all-star candidate team split hook already installed target=%p", target);
        ok = 1;
    } else if (memcmp(target, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star candidate team split hook", target, sizeof(april_expected));
    } else {
        void* return_address = target + sizeof(april_expected);
        void* seeded_address = target + (OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_DONE_RVA - OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_SITE_RVA);
        void* vector_push_back_address = resolve_relative_call_target(target + 0xA5);
        if (vector_push_back_address == NULL) {
            OotpBuildInfo info = read_ootp_build_info();
            uint32_t vector_rva = (info.ok && info.timestamp == 0x69F75E6Bu) ? 0x01CBAA40u : OOTP27_VECTOR_PUSH_BACK_RVA;
            vector_push_back_address = (void*)((uintptr_t)exe + vector_rva);
        }

        uint8_t* stub = build_allstar_candidate_team_split_stub(
            return_address,
            seeded_address,
            vector_push_back_address,
            layout.subleague_array_offset,
            layout.subleague_count_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star candidate team split hook stub");
        } else {
            uint8_t patch[27] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                0x90, 0x90, 0x90, 0x90, 0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star candidate team split hook error=%lu", GetLastError());
            } else {
                memcpy(target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(target, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star candidate team split hook target=%p stub=%p return=%p seeded=%p vector_push=%p helper=%p",
                    target,
                    stub,
                    return_address,
                    seeded_address,
                    vector_push_back_address,
                    &ootp_kbo_seed_single_division_allstar_candidate_teams);
                ok = 1;
            }
        }
    }

    return ok;
}

static int install_allstar_voting_begin_prepare_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star voting begin prepare patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[16] = {
        0x8D, 0x53, 0xE4,
        0x80, 0xBE, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0xF3, 0x01, 0x00, 0x00
    };
    const uint8_t april_expected[16] = {
        0x8D, 0x53, 0xE4,
        0x80, 0xBE, 0xE8, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0xF3, 0x01, 0x00, 0x00
    };

    uint8_t* target = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    const uint8_t* expected = may_expected;
    if (target == NULL) {
        target = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }
    if (target == NULL) {
        target = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_VOTING_BEGIN_PREP_SITE_RVA);
        expected = april_expected;
    }

    if (!memory_range_readable(target, sizeof(april_expected))) {
        append_logf("KBO all-star voting begin prepare hook target unreadable target=%p", target);
        return 0;
    }

    int ok = 0;
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        append_logf("KBO all-star voting begin prepare hook already installed target=%p", target);
        ok = 1;
    } else if (memcmp(target, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star voting begin prepare hook", target, sizeof(april_expected));
    } else {
        void* return_address = target + sizeof(april_expected);
        void* no_game_address = target + (OOTP27_ALLSTAR_VOTING_BEGIN_NO_GAME_RVA - OOTP27_ALLSTAR_VOTING_BEGIN_PREP_SITE_RVA);
        uint8_t* stub = build_allstar_voting_begin_prepare_stub(return_address, no_game_address, NULL, layout.game_flag_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star voting begin prepare hook stub");
        } else {
            uint8_t patch[16] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star voting begin prepare hook error=%lu", GetLastError());
            } else {
                memcpy(target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(target, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star voting begin prepare hook target=%p stub=%p return=%p no_game=%p helper=%p",
                    target,
                    stub,
                    return_address,
                    no_game_address,
                    &ootp_kbo_prepare_allstar_voting_begin);
                ok = 1;
            }
        }
    }

    return ok;
}

static int install_allstar_events_prepare_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star events prepare patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[19] = {
        0x41, 0x80, 0xBC, 0x24, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x08,
        0x49, 0x8B, 0xCC,
        0xE8, 0xB9, 0x55, 0xFF, 0xFF
    };
    const uint8_t april_expected[19] = {
        0x41, 0x80, 0xBC, 0x24, 0xE8, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x08,
        0x49, 0x8B, 0xCC,
        0xE8, 0x89, 0x55, 0xFF, 0xFF
    };

    uint8_t* target = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    const uint8_t* expected = may_expected;
    if (target == NULL) {
        target = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }
    if (target == NULL) {
        target = (uint8_t*)((uintptr_t)exe + OOTP27_MAKE_ALLSTAR_EVENTS_PREP_SITE_RVA);
        expected = april_expected;
    }

    if (!memory_range_readable(target, sizeof(april_expected))) {
        append_logf("KBO all-star events prepare hook target unreadable target=%p", target);
        return 0;
    }

    int ok = 0;
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        append_logf("KBO all-star events prepare hook already installed target=%p", target);
        ok = 1;
    } else if (memcmp(target, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star events prepare hook", target, sizeof(april_expected));
    } else {
        void* return_address = target + sizeof(april_expected);
        void* allstar_prep_address = resolve_relative_call_target(target + 14);
        if (allstar_prep_address == NULL) {
            append_log_line("KBO all-star events prepare hook skipped: prep call target unresolved");
            return 0;
        }

        uint8_t* stub = build_allstar_events_prepare_stub(return_address, allstar_prep_address, layout.game_flag_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star events prepare hook stub");
        } else {
            uint8_t patch[19] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90, 0x90, 0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star events prepare hook error=%lu", GetLastError());
            } else {
                memcpy(target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(target, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star events prepare hook target=%p stub=%p return=%p prep=%p helper=%p",
                    target,
                    stub,
                    return_address,
                    allstar_prep_address,
                    &ootp_kbo_prepare_allstar_events);
                ok = 1;
            }
        }
    }

    return ok;
}

static int install_allstar_settings_ui_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star settings UI patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[16] = {
        0x41, 0x0F, 0xB6, 0x95, 0xF0, 0x45, 0x00, 0x00,
        0x48, 0x8B, 0xCF,
        0xE8, 0x7C, 0xBC, 0x92, 0x00
    };
    const uint8_t april_expected[16] = {
        0x41, 0x0F, 0xB6, 0x95, 0xE8, 0x45, 0x00, 0x00,
        0x48, 0x8B, 0xCF,
        0xE8, 0xB9, 0x60, 0x93, 0x00
    };

    uint8_t* value_site = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    const uint8_t* expected = may_expected;
    if (value_site == NULL) {
        value_site = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }
    if (value_site == NULL) {
        value_site = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_SETTINGS_ENABLE_VALUE_SITE_RVA);
        expected = april_expected;
    }

    int ok_value = 0;
    if (!memory_range_readable(value_site, sizeof(april_expected))) {
        append_logf("KBO all-star settings value hook target unreadable target=%p", value_site);
    } else if (is_rip_absolute_jump_patch(value_site) || is_rax_absolute_jump_patch(value_site)) {
        append_logf("KBO all-star settings value hook already installed target=%p", value_site);
        ok_value = 1;
    } else if (memcmp(value_site, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star settings value hook", value_site, sizeof(april_expected));
    } else {
        void* return_address = value_site + sizeof(april_expected);
        void* checkbox_set_bool_address = resolve_relative_call_target(value_site + 11);
        if (checkbox_set_bool_address == NULL) {
            OotpBuildInfo info = read_ootp_build_info();
            uint32_t checkbox_rva = (info.ok && info.timestamp == 0x69F75E6Bu) ? 0x01E717D0u : OOTP27_UI_CHECKBOX_SET_BOOL_RVA;
            checkbox_set_bool_address = (void*)((uintptr_t)exe + checkbox_rva);
        }

        uint8_t* stub = build_allstar_settings_enable_stub(return_address, checkbox_set_bool_address, layout.game_flag_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star settings value hook stub");
        } else {
            uint8_t patch[16] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(value_site, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star settings value hook error=%lu", GetLastError());
            } else {
                memcpy(value_site, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), value_site, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(value_site, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star settings value hook target=%p stub=%p return=%p checkbox_set_bool=%p helper=%p",
                    value_site,
                    stub,
                    return_address,
                    checkbox_set_bool_address,
                    &ootp_kbo_enable_allstar_setting);
                ok_value = 1;
            }
        }
    }

    const uint8_t disable_context_may[] = {
        0x41, 0x80, 0xBD, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0x2A, 0x15, 0x00, 0x00
    };
    const uint8_t disable_expected_may[6] = {
        0x0F, 0x84, 0x2A, 0x15, 0x00, 0x00
    };
    const uint8_t disable_patch_may[6] = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    int ok_disable = kbo_patch_static_pattern(
        "KBO all-star settings single-division disable gate",
        disable_context_may,
        sizeof(disable_context_may),
        8,
        disable_expected_may,
        disable_patch_may,
        sizeof(disable_expected_may));
    if (!ok_disable) {
        uint8_t* disable_gate = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_SETTINGS_SINGLE_DIVISION_DISABLE_RVA);
        const uint8_t expected_disable_gate[2] = {0x74, 0x44};
        const uint8_t patch_disable_gate[2] = {0xEB, 0x44};
        if (memory_range_readable(disable_gate, sizeof(expected_disable_gate))) {
            ok_disable = patch_static_bytes(
                "KBO all-star settings single-division disable gate",
                disable_gate,
                expected_disable_gate,
                patch_disable_gate,
                sizeof(expected_disable_gate));
        }
    }

    const uint8_t one_division_warning_context_may[] = {
        0x41, 0x80, 0xBD, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0x2A, 0x15, 0x00, 0x00,
        0x66, 0x83, 0xBB, 0xFA, 0x03, 0x00, 0x00, 0x01,
        0x0F, 0x8C
    };
    const uint8_t one_division_warning_expected_may[6] = {
        0x0F, 0x8C, 0x87, 0x0B, 0x00, 0x00
    };
    const uint8_t one_division_warning_patch_may[6] = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    int ok_warning = kbo_patch_static_pattern(
        "KBO all-star settings one-division warning gate",
        one_division_warning_context_may,
        sizeof(one_division_warning_context_may),
        22,
        one_division_warning_expected_may,
        one_division_warning_patch_may,
        sizeof(one_division_warning_expected_may));
    if (!ok_warning) {
        uint8_t* warning_gate = (uint8_t*)((uintptr_t)exe + 0x01545BB8u);
        const uint8_t expected_warning_gate[14] = {
            0x66, 0x83, 0xBB, 0xFA, 0x03, 0x00, 0x00, 0x01,
            0x0F, 0x8C, 0x87, 0x0B, 0x00, 0x00
        };
        const uint8_t patch_warning_gate[14] = {
            0x66, 0x83, 0xBB, 0xFA, 0x03, 0x00, 0x00, 0x01,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90
        };
        if (memory_range_readable(warning_gate, sizeof(expected_warning_gate))) {
            ok_warning = patch_static_bytes(
                "KBO all-star settings one-division warning gate",
                warning_gate,
                expected_warning_gate,
                patch_warning_gate,
                sizeof(expected_warning_gate));
        }
    }

    return ok_value || ok_disable || ok_warning;
}
