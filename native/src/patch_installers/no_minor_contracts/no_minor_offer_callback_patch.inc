static int install_kbo_no_minor_contract_offer_callback_probe_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const size_t stolen_len = 20;
    const uint8_t expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,                   /* mov [rsp+0x8],rbx */
        0x48, 0x89, 0x6C, 0x24, 0x10,                   /* mov [rsp+0x10],rbp */
        0x48, 0x89, 0x74, 0x24, 0x20,                   /* mov [rsp+0x20],rsi */
        0x57,                                           /* push rdi */
        0x48, 0x83, 0xEC, 0x20                          /* sub rsp,0x20 */
    };
    const uint8_t context[48] = {
        0x00, 0x00, 0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D,
        0x41, 0x5C, 0x5F, 0x5E, 0x5D, 0xC3, 0xCC, 0xCC,
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57,
        0x48, 0x83, 0xEC, 0x20, 0x49, 0x8B, 0xF1, 0x49,
        0x8B, 0xF8, 0x48, 0x8B, 0xEA, 0x48, 0x8B, 0xD9
    };

    uint8_t* target = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_FA_OFFER_SCREEN_CALLBACK_PROBE_RVA,
        expected,
        sizeof(expected),
        context,
        sizeof(context),
        16u,
        "KBO no-minor offer callback probe");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor offer callback probe already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO no-minor offer callback probe trampoline");
        return 0;
    }
    uint8_t* stub = build_kbo_fa_offer_screen_callback_probe_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor offer callback probe detour stub");
        return 0;
    }

    uint8_t patch[20] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor offer callback probe error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO no-minor offer callback probe target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_fa_offer_screen_callback_probe_wrapper);
    return 1;
}
