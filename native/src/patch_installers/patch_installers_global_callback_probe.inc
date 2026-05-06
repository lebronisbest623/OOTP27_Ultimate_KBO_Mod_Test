static int install_kbo_global_handle_callback_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO global callback probe patch");
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_GLOBAL_HANDLE_CALLBACK_RVA);
    const size_t stolen_len = 18;
    uint8_t expected[18] = {
        0x48, 0x8B, 0xC4,                               // mov rax, rsp
        0x48, 0x89, 0x58, 0x08,                         // mov [rax + 8], rbx
        0x4C, 0x89, 0x40, 0x18,                         // mov [rax + 0x18], r8
        0x55,                                           // push rbp
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55                                      // push r13
    };

    if (target[0] == 0x48 && target[1] == 0xB8 && target[10] == 0xFF && target[11] == 0xE0) {
        append_logf("KBO global callback probe patch already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO global callback probe patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO global callback probe trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_global_handle_callback_probe_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO global callback probe detour stub");
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
        append_logf("VirtualProtect failed for KBO global callback probe patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf("installed KBO global callback probe patch target=%p stub=%p trampoline=%p wrapper=%p",
        target, stub, trampoline, &ootp_kbo_global_handle_callback_probe_wrapper);
    return 1;
}

