static uint8_t* build_kbo_intl_established_fa_count_stub(void* continuation, void* progress_title)
{
    uint8_t code[96] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x8B, 0xCF,                                     // mov ecx, edi
        0x49, 0x8B, 0xD4,                               // mov rdx, r12
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x8B, 0xF8,                                     // mov edi, eax
        0x89, 0x7D, 0x98,                               // mov [rbp-0x68], edi
        0x45, 0x33, 0xC9,                               // xor r9d, r9d
        0x49, 0xB8,                                     // mov r8, progress_title
        0,0,0,0,0,0,0,0,
        0x41, 0x8D, 0x51, 0x01,                         // lea edx, [r9+1]
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[11], (uint64_t)(uintptr_t)&ootp_kbo_intl_established_fa_count_wrapper);
    write_u64(&code[35], (uint64_t)(uintptr_t)progress_title);
    write_u64(&code[49], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_intl_established_fa_player_probe_stub(void* continuation)
{
    uint8_t code[96] = {
        0x48, 0x83, 0xEC, 0x30,                         // sub rsp, 0x30
        0x48, 0x89, 0x44, 0x24, 0x28,                   // mov [rsp+0x28], rax
        0x48, 0x8B, 0xC8,                               // mov rcx, rax
        0x49, 0x8B, 0xD4,                               // mov rdx, r12
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x8B, 0x44, 0x24, 0x28,                   // mov rax, [rsp+0x28]
        0x48, 0x83, 0xC4, 0x30,                         // add rsp, 0x30
        0x48, 0x8B, 0xD8,                               // mov rbx, rax
        0x48, 0x89, 0x45, 0x08,                         // mov [rbp+0x8], rax
        0x0F, 0x57, 0xC0,                               // xorps xmm0, xmm0
        0x33, 0xC0,                                     // xor eax, eax
        0x0F, 0x11, 0x83, 0x38, 0x08, 0x00, 0x00,       // movups [rbx+0x838], xmm0
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC
    };

    write_u64(&code[17], (uint64_t)(uintptr_t)&ootp_kbo_intl_established_fa_player_probe_wrapper);
    write_u64(&code[57], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
