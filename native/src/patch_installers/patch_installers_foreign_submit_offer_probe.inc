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
    const uint8_t context[48] = {
        0x8B, 0x7B, 0x48, 0x49, 0x8B, 0xE3, 0x41, 0x5F,
        0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5D, 0xC3,
        0x48, 0x89, 0x5C, 0x24, 0x20, 0x48, 0x89, 0x4C,
        0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41,
        0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C,
        0x24, 0xD9, 0x48, 0x81, 0xEC, 0xE0, 0x00, 0x00
    };

    uint8_t* target = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_FA_SUBMIT_OFFER_PROBE_RVA,
        expected,
        sizeof(expected),
        context,
        sizeof(context),
        16u,
        "KBO FA submit-offer probe patch");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO FA submit-offer probe patch already installed target=%p", target);
        return 1;
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
