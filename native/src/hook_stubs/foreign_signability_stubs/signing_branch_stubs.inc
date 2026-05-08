static uint8_t* build_kbo_fa_signing_branch_detour_stub(void* original_trampoline)
{
    uint8_t code[96] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x49, 0x8B, 0xCF,                               // mov rcx, r15
        0x49, 0x8B, 0xD4,                               // mov rdx, r12
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x85, 0xC0,                                     // test eax, eax
        0x75, 0x03,                                     // jne allow
        0x4D, 0x33, 0xE4,                               // xor r12, r12
        0x48, 0x83, 0xC4, 0x28,                         // add rsp, 0x28
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x58,                                           // pop rax
        0x48, 0xB8,                                     // mov rax, original_trampoline
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC
    };

    write_u64(&code[23], (uint64_t)(uintptr_t)&ootp_kbo_fa_signing_branch_wrapper);
    write_u64(&code[57], (uint64_t)(uintptr_t)original_trampoline);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_fa_signing_success_post_stub(void* continuation, void* patch_site)
{
    uint8_t code[96] = {
        0x41, 0xC6, 0x45, 0x68, 0x01,                   // mov byte ptr [r13+0x68],1
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x49, 0x8B, 0xCF,                               // mov rcx,r15
        0x49, 0x8B, 0xD4,                               // mov rdx,r12
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x28,                         // add rsp, 0x28
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x58,                                           // pop rax
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[31], (uint64_t)(uintptr_t)&ootp_kbo_fa_signing_success_post_wrapper);
    write_u64(&code[65], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = kbo_alloc_near_code(patch_site, sizeof(code));
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
