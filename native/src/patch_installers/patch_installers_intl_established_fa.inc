static int install_kbo_intl_established_fa_multiplier_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO international established FA multiplier patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO international established FA multiplier patch host=%s", host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_INTL_ESTABLISHED_FA_COUNT_READY_RVA);
    const uint8_t expected[14] = {
        0x45, 0x33, 0xC9,                               /* xor r9d,r9d */
        0x4C, 0x8D, 0x05, 0x76, 0x98, 0x41, 0x02,       /* lea r8,[Creating International Free Agents] */
        0x41, 0x8D, 0x51, 0x01                          /* lea edx,[r9+1] */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO international established FA multiplier patch target unreadable target=%p", target);
        return 0;
    }
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        append_logf("KBO international established FA multiplier patch already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO international established FA multiplier patch", target, sizeof(expected));
        return 0;
    }

    void* continuation = target + sizeof(expected);
    void* progress_title = (void*)((uintptr_t)exe + OOTP27_INTL_ESTABLISHED_FA_PROGRESS_TITLE_RVA);
    uint8_t* stub = build_kbo_intl_established_fa_count_stub(continuation, progress_title);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO international established FA multiplier stub");
        return 0;
    }

    uint8_t patch[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO international established FA multiplier patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO international established FA multiplier patch target=%p stub=%p continuation=%p wrapper=%p multiplier=%d",
        target,
        stub,
        continuation,
        &ootp_kbo_intl_established_fa_count_wrapper,
        kbo_get_intl_established_fa_multiplier());
    return 1;
}

static int install_kbo_intl_established_fa_player_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO international established FA player probe patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO international established FA player probe patch host=%s", host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_INTL_ESTABLISHED_FA_PLAYER_PROBE_RVA);
    const uint8_t expected[OOTP27_INTL_ESTABLISHED_FA_PLAYER_PROBE_STOLEN_LEN] = {
        0x48, 0x8B, 0xD8,                               /* mov rbx,rax */
        0x48, 0x89, 0x45, 0x08,                         /* mov [rbp+0x8],rax */
        0x0F, 0x57, 0xC0,                               /* xorps xmm0,xmm0 */
        0x33, 0xC0,                                     /* xor eax,eax */
        0x0F, 0x11, 0x83, 0x38, 0x08, 0x00, 0x00        /* movups [rbx+0x838],xmm0 */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO international established FA player probe patch target unreadable target=%p", target);
        return 0;
    }
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        append_logf("KBO international established FA player probe patch already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO international established FA player probe patch", target, sizeof(expected));
        return 0;
    }

    void* continuation = target + sizeof(expected);
    uint8_t* stub = build_kbo_intl_established_fa_player_probe_stub(continuation);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO international established FA player probe stub");
        return 0;
    }

    uint8_t patch[OOTP27_INTL_ESTABLISHED_FA_PLAYER_PROBE_STOLEN_LEN] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0,
        0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO international established FA player probe patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO international established FA player probe patch target=%p stub=%p continuation=%p wrapper=%p",
        target,
        stub,
        continuation,
        &ootp_kbo_intl_established_fa_player_probe_wrapper);
    return 1;
}
