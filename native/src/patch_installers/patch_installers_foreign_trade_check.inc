static int install_kbo_trade_check_foreign_policy_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO trade foreign policy patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO trade foreign policy patch host=%s", host);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_LEAGUE_TRADE_CHECK_RVA);
    const size_t stolen_len = 18;
    const uint8_t expected[18] = {
        0x48, 0x8B, 0xC4,                               /* mov rax,rsp */
        0x44, 0x88, 0x48, 0x20,                         /* mov [rax+0x20],r9b */
        0x4C, 0x89, 0x40, 0x18,                         /* mov [rax+0x18],r8 */
        0x89, 0x50, 0x10,                               /* mov [rax+0x10],edx */
        0x48, 0x89, 0x48, 0x08                          /* mov [rax+0x8],rcx */
    };

    if (!memory_range_readable(target, stolen_len)) {
        append_logf("KBO trade foreign policy patch target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO trade foreign policy patch already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO trade foreign policy patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO trade foreign policy trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_trade_check_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO trade foreign policy detour stub");
        return 0;
    }

    uint8_t patch[18] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO trade foreign policy patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO trade foreign policy patch target=%p rva=0x%llx stub=%p trampoline=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        trampoline,
        &ootp_kbo_trade_check_foreign_policy_probe);
    return 1;
}
