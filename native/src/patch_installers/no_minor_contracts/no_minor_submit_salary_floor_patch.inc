static int install_kbo_no_minor_contract_submit_salary_floor_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_SUBMIT_SALARY_FLOOR_13B0712_RVA);
    const uint8_t expected[26] = {
        0x38, 0x9F, 0xCC, 0x00, 0x00, 0x00,             /* cmp [rdi+0xcc], bl */
        0x0F, 0x84, 0xDD, 0x01, 0x00, 0x00,             /* je 0x1413b08fb */
        0x8B, 0x87, 0xC0, 0x00, 0x00, 0x00,             /* mov eax, [rdi+0xc0] */
        0x85, 0xC0,                                     /* test eax, eax */
        0x0F, 0x85, 0x8F, 0x00, 0x00, 0x00              /* jne 0x1413b07bb */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO no-minor submit salary floor target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor submit salary floor already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor submit salary floor", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_contract_submit_salary_floor_stub(
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_SUBMIT_ZERO_ERROR_13B072C_RVA),
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_SUBMIT_MIN_CHECK_13B07BB_RVA),
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_SUBMIT_AFTER_MIN_CHECK_13B0860_RVA));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor submit salary floor detour stub");
        return 0;
    }

    uint8_t patch[26] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor submit salary floor error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO no-minor submit salary floor target=%p stub=%p floor_source=[r14+0x278]",
        target,
        stub);
    return 1;
}

