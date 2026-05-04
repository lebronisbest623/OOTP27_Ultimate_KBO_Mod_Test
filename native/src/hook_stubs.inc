static uint8_t* build_kbo_military_service_entry_trampoline(void* original_address, size_t stolen_len)
{
    if (original_address == NULL || stolen_len == 0 || stolen_len > 64) {
        return NULL;
    }

    size_t code_len = stolen_len + 12;
    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, code_len, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }

    memcpy(memory, original_address, stolen_len);
    memory[stolen_len + 0] = 0x48;                     // mov rax, original + stolen_len
    memory[stolen_len + 1] = 0xB8;
    write_u64(&memory[stolen_len + 2], (uint64_t)((uintptr_t)original_address + stolen_len));
    memory[stolen_len + 10] = 0xFF;                    // jmp rax
    memory[stolen_len + 11] = 0xE0;
    FlushInstructionCache(GetCurrentProcess(), memory, code_len);
    return memory;
}

static uint8_t* build_kbo_military_service_entry_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x48, 0xBA,                                     // mov rdx, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_military_service_entry_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_military_status_update_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x48, 0xBA,                                     // mov rdx, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_military_status_update_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_player_team_signability_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x49, 0xB9,                                     // mov r9, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_player_team_signability_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_player_offer_eligibility_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x49, 0xB9,                                     // mov r9, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_player_offer_eligibility_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_fa_submit_offer_probe_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x48, 0xBA,                                     // mov rdx, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_fa_submit_offer_probe_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_ai_fa_status_candidate_insert_stub(void* continuation)
{
    uint8_t code[80] = {
        0x41, 0x89, 0xC8,                               // mov r8d, ecx
        0x48, 0x89, 0xE9,                               // mov rcx, rbp
        0x4C, 0x89, 0xFA,                               // mov rdx, r15
        0x4D, 0x89, 0xF1,                               // mov r9, r14
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x89, 0xC1,                                     // mov ecx, eax
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[14], (uint64_t)(uintptr_t)&ootp_kbo_ai_fa_status_candidate_insert_wrapper);
    write_u64(&code[28], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
