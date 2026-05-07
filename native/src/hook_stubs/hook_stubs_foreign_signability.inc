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

static uint8_t* build_kbo_fa_offer_player_demand_floor_17a79bb_stub(void* continuation)
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
        0x49, 0x8B, 0xCD,                               // mov rcx, r13 (player)
        0x49, 0x8B, 0xD6,                               // mov rdx, r14 (screen)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x48, 0xB8,                                     // mov rax, probe
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
        0x66, 0x89, 0x75, 0x98,                         // mov [rbp-0x68], si
        0x49, 0x89, 0xB6, 0xD8, 0x00, 0x00, 0x00,       // mov [r14+0xd8], rsi
        0x8D, 0x4E, 0x04,                               // lea ecx, [rsi+4]
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u32(&code[23], OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA);
    write_u64(&code[29], (uint64_t)(uintptr_t)&ootp_kbo_fa_offer_player_demand_floor_probe);
    write_u64(&code[70], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_fa_offer_player_demand_floor_17b50b4_stub(void* continuation)
{
    uint8_t code[112] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x49, 0x8B, 0xCE,                               // mov rcx, r14 (player)
        0x49, 0x8B, 0xD5,                               // mov rdx, r13 (screen)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x48, 0xB8,                                     // mov rax, probe
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
        0x45, 0x38, 0xBE, 0xC3, 0x0D, 0x00, 0x00,       // cmp r15b, [r14+0xdc3]
        0x74, 0x0B,                                     // je continuation block
        0x41, 0xC7, 0x85, 0x1C, 0x03, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00,                         // mov [r13+0x31c], 8
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u32(&code[23], OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA);
    write_u64(&code[29], (uint64_t)(uintptr_t)&ootp_kbo_fa_offer_player_demand_floor_probe);
    write_u64(&code[76], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_no_minor_demand_write_floor_aab739_stub(
    void* notify_call,
    void* done_target)
{
    uint8_t code[112] = {
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x48, 0x8B, 0xCB,                               // mov rcx, rbx (player)
        0x8B, 0xD0,                                     // mov edx, eax (proposed demand)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x45, 0x8B, 0x8D, 0x78, 0x02, 0x00, 0x00,       // mov r9d, [r13+0x278] (salary floor hint)
        0x48, 0xB8,                                     // mov rax, probe
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x89, 0x83, 0x84, 0x0D, 0x00, 0x00,             // mov [rbx+0xd84], eax
        0x48, 0xB8,                                     // mov rax, original notify call target
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0xB8,                                     // mov rax, original done target
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u32(&code[21], OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_RVA);
    write_u64(&code[34], (uint64_t)(uintptr_t)&ootp_kbo_no_minor_demand_write_floor_probe);
    write_u64(&code[66], (uint64_t)(uintptr_t)notify_call);
    write_u64(&code[78], (uint64_t)(uintptr_t)done_target);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_no_minor_demand_write_floor_1077952_stub(void* continuation)
{
    uint8_t code[96] = {
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x8B, 0xD0,                                     // mov edx, eax (proposed demand)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x45, 0x33, 0xC9,                               // xor r9d, r9d
        0x48, 0xB8,                                     // mov rax, probe
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x89, 0x81, 0x84, 0x0D, 0x00, 0x00,             // mov [rcx+0xd84], eax
        0x41, 0x8B, 0x97, 0x20, 0x01, 0x00, 0x00,       // mov edx, [r15+0x120]
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u32(&code[18], OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_1077952_RVA);
    write_u64(&code[27], (uint64_t)(uintptr_t)&ootp_kbo_no_minor_demand_write_floor_probe);
    write_u64(&code[66], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_fa_offer_screen_callback_probe_detour_stub(void* original_trampoline)
{
    uint8_t code[64] = {
        0x48, 0x83, 0xEC, 0x38,                         // sub rsp, 0x38
        0x48, 0xB8,                                     // mov rax, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0x89, 0x44, 0x24, 0x20,                   // mov [rsp+0x20], rax
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x38,                         // add rsp, 0x38
        0xC3,                                           // ret
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[6], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[21], (uint64_t)(uintptr_t)&ootp_kbo_fa_offer_screen_callback_probe_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_fa_contract_offer_callback_probe_detour_stub(void* original_trampoline)
{
    uint8_t code[64] = {
        0x48, 0x83, 0xEC, 0x38,                         // sub rsp, 0x38
        0x48, 0xB8,                                     // mov rax, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0x89, 0x44, 0x24, 0x20,                   // mov [rsp+0x20], rax
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x38,                         // add rsp, 0x38
        0xC3,                                           // ret
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[6], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[21], (uint64_t)(uintptr_t)&ootp_kbo_fa_contract_offer_callback_probe_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_player_action_eligibility_detour_stub(void* original_trampoline)
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
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_player_action_eligibility_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_no_minor_contract_submit_salary_floor_stub(
    void* zero_salary_continuation,
    void* min_check_continuation,
    void* after_min_check_continuation)
{
    uint8_t code[96] = {
        0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x01,       // mov byte ptr [rdi+0xcc], 1
        0x8B, 0x87, 0xC0, 0x00, 0x00, 0x00,             // mov eax, [rdi+0xc0]
        0x4D, 0x85, 0xF6,                               // test r14, r14
        0x74, 0x21,                                     // je fallback_to_original
        0x41, 0x8B, 0x8E, 0x78, 0x02, 0x00, 0x00,       // mov ecx, [r14+0x278]
        0x85, 0xC9,                                     // test ecx, ecx
        0x7E, 0x16,                                     // jle fallback_to_original
        0x3B, 0xC1,                                     // cmp eax, ecx
        0x7D, 0x06,                                     // jge continue_after_min_check
        0x89, 0x8F, 0xC0, 0x00, 0x00, 0x00,             // mov [rdi+0xc0], ecx
        0x48, 0xB8,                                     // mov rax, after_min_check_continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x85, 0xC0,                                     // test eax, eax
        0x75, 0x0C,                                     // jne continue_min_check
        0x48, 0xB8,                                     // mov rax, zero_salary_continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0xB8,                                     // mov rax, min_check_continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC
    };

    write_u64(&code[41], (uint64_t)(uintptr_t)after_min_check_continuation);
    write_u64(&code[57], (uint64_t)(uintptr_t)zero_salary_continuation);
    write_u64(&code[69], (uint64_t)(uintptr_t)min_check_continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

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
