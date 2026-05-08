static uint8_t* build_kbo_trade_check_detour_stub(void* original_trampoline)
{
    uint8_t code[64] = {
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x48, 0xB8,                                     // mov rax, probe
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x28,                         // add rsp, 0x28
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x85, 0xC0,                                     // test eax, eax
        0x74, 0x0C,                                     // jz blocked
        0x48, 0xB8,                                     // mov rax, original_trampoline
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xB8, 0xF7, 0xFF, 0xFF, 0xFF,                   // mov eax, -9
        0xC3,                                           // ret
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_trade_check_foreign_policy_probe);
    write_u64(&code[38], (uint64_t)(uintptr_t)original_trampoline);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
