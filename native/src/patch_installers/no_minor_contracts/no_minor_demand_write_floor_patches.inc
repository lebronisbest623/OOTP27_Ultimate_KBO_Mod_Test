static int install_kbo_no_minor_contract_demand_write_floor_aab739_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_RVA);
    const uint8_t expected[13] = {
        0x89, 0x83, 0x84, 0x0D, 0x00, 0x00,             /* mov [rbx+0xd84],eax */
        0xE8, 0xFC, 0xF2, 0x20, 0x01,                   /* call 0x141cbaa40 */
        0xEB, 0x4E                                      /* jmp 0x140aab794 */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO no-minor demand write floor AAB739 target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor demand write floor AAB739 already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor demand write floor AAB739", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_demand_write_floor_aab739_stub(
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_NOTIFY_RVA),
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_DONE_RVA));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor demand write floor AAB739 detour stub");
        return 0;
    }

    uint8_t patch[13] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor demand write floor AAB739 error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf("installed KBO no-minor demand write floor AAB739 target=%p stub=%p", target, stub);
    return 1;
}

static int install_kbo_no_minor_contract_demand_write_floor_1077952_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_1077952_RVA);
    const uint8_t expected[13] = {
        0x89, 0x81, 0x84, 0x0D, 0x00, 0x00,             /* mov [rcx+0xd84],eax */
        0x41, 0x8B, 0x97, 0x20, 0x01, 0x00, 0x00        /* mov edx,[r15+0x120] */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO no-minor demand write floor 1077952 target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor demand write floor 1077952 already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor demand write floor 1077952", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_demand_write_floor_1077952_stub(target + sizeof(expected));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor demand write floor 1077952 detour stub");
        return 0;
    }

    uint8_t patch[13] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor demand write floor 1077952 error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf("installed KBO no-minor demand write floor 1077952 target=%p stub=%p", target, stub);
    return 1;
}

