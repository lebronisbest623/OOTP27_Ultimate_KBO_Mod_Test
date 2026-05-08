static int install_kbo_no_minor_contract_player_action_eligibility_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_PLAYER_ACTION_ELIGIBILITY_RVA);
    const size_t stolen_len = 21;
    const uint8_t expected[21] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,                   /* mov [rsp+0x8],rbx */
        0x44, 0x88, 0x44, 0x24, 0x18,                   /* mov [rsp+0x18],r8b */
        0x55,                                           /* push rbp */
        0x56,                                           /* push rsi */
        0x57,                                           /* push rdi */
        0x41, 0x54,                                     /* push r12 */
        0x41, 0x55,                                     /* push r13 */
        0x41, 0x56,                                     /* push r14 */
        0x41, 0x57                                      /* push r15 */
    };

    if (!memory_range_readable(target, stolen_len)) {
        append_logf("KBO no-minor player action eligibility target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor player action eligibility already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor player action eligibility", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO no-minor player action eligibility trampoline");
        return 0;
    }
    uint8_t* stub = build_kbo_player_action_eligibility_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor player action eligibility detour stub");
        return 0;
    }

    uint8_t patch[21] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor player action eligibility error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO no-minor player action eligibility target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_player_action_eligibility_wrapper);
    return 1;
}

