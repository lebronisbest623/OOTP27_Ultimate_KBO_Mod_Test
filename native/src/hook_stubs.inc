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

static uint8_t* build_kbo_active_foreign_hitter_count_detour_stub(void* original_trampoline)
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
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_active_foreign_hitter_count_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_active_foreign_pitcher_count_detour_stub(void* original_trampoline)
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
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_active_foreign_pitcher_count_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_kbo_callup_foreign_limit_branch_stub(
    void* allow_continuation,
    void* block_continuation,
    void* fail_continuation,
    void* wrapper,
    int total_check)
{
    uint8_t code[112] = {
        0x45, 0x89, 0xC8,                               // mov r8d,r9d placeholder
        0x41, 0x89, 0xD1,                               // mov r9d,edx placeholder
        0x48, 0x89, 0xFA,                               // mov rdx,rdi
        0x49, 0x8B, 0xCF,                               // mov rcx,r15
        0x48, 0xB8,                                     // mov rax,wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x84, 0xC0,                                     // test al,al
        0x74, 0x0C,                                     // je not_allowed
        0x48, 0xB8,                                     // mov rax,allow
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x45, 0x84, 0xE4,                               // test r12b,r12b
        0x74, 0x0C,                                     // je block
        0x48, 0xB8,                                     // mov rax,fail
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0xB8,                                     // mov rax,block
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    if (total_check) {
        code[0] = 0x41; code[1] = 0x89; code[2] = 0xD0; /* mov r8d,edx */
        code[3] = 0x41; code[4] = 0x89; code[5] = 0xC9; /* mov r9d,ecx */
    } else {
        code[0] = 0x41; code[1] = 0x89; code[2] = 0xC0; /* mov r8d,eax */
        code[3] = 0x41; code[4] = 0x89; code[5] = 0xD1; /* mov r9d,edx */
    }

    write_u64(&code[14], (uint64_t)(uintptr_t)wrapper);
    write_u64(&code[30], (uint64_t)(uintptr_t)allow_continuation);
    write_u64(&code[47], (uint64_t)(uintptr_t)fail_continuation);
    write_u64(&code[59], (uint64_t)(uintptr_t)block_continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_allstar_settings_enable_stub(void* return_address, void* checkbox_set_bool_address, uint32_t game_flag_offset)
{
    (void)game_flag_offset;

    uint8_t code[64] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x49, 0x8B, 0xCD,                               // mov rcx, r13
        0x48, 0xB8,                                     // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0xBA, 0x01, 0x00, 0x00, 0x00,                   // mov edx, 1
        0x90, 0x90, 0x90,                               // pad original movzx length
        0x48, 0x8B, 0xCF,                               // mov rcx, rdi
        0x48, 0xB8,                                     // mov rax, checkbox_set_bool_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[9], (uint64_t)(uintptr_t)&ootp_kbo_enable_allstar_setting);
    write_u64(&code[36], (uint64_t)(uintptr_t)checkbox_set_bool_address);
    write_u64(&code[48], (uint64_t)(uintptr_t)return_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_allstar_events_prepare_stub(void* return_address, void* allstar_prep_address, uint32_t game_flag_offset)
{
    uint8_t code[73] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x49, 0x8B, 0xCC,                               // mov rcx, r12
        0x48, 0xB8,                                     // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x41, 0x80, 0xBC, 0x24, 0,0,0,0, 0x00,          // cmp byte ptr [r12 + game_flag_offset], 0
        0x75, 0x0C,                                     // jne call_prep
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x49, 0x8B, 0xCC,                               // call_prep: mov rcx, r12
        0x48, 0xB8,                                     // mov rax, allstar_prep_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[9], (uint64_t)(uintptr_t)&ootp_kbo_prepare_allstar_events);
    write_u32(&code[27], game_flag_offset);
    write_u64(&code[36], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[51], (uint64_t)(uintptr_t)allstar_prep_address);
    write_u64(&code[63], (uint64_t)(uintptr_t)return_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_allstar_voting_begin_prepare_stub(void* return_address, void* no_game_address, void* allstar_team_setup_address, uint32_t game_flag_offset)
{
    uint8_t code[91] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x48, 0x8B, 0xCE,                               // mov rcx, rsi
        0x48, 0xBA,                                     // mov rdx, allstar_team_setup_address
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, helper
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
        0x8D, 0x53, 0xE4,                               // lea edx, [rbx - 0x1c]
        0x80, 0xBE, 0,0,0,0, 0x00,                      // cmp byte ptr [rsi + game_flag_offset], 0
        0x74, 0x0C,                                     // je no_game
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0xB8,                                     // no_game: mov rax, no_game_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[20], (uint64_t)(uintptr_t)allstar_team_setup_address);
    write_u64(&code[30], (uint64_t)(uintptr_t)&ootp_kbo_prepare_allstar_voting_begin);
    write_u32(&code[60], game_flag_offset);
    write_u64(&code[69], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[81], (uint64_t)(uintptr_t)no_game_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

static uint8_t* build_allstar_candidate_team_split_stub(
    void* return_address,
    void* seeded_address,
    void* vector_push_back_address,
    uint32_t subleague_array_offset,
    uint32_t subleague_count_offset)
{
    uint8_t code[96] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x49, 0x8B, 0xCE,                               // mov rcx, r14
        0x48, 0x8D, 0x55, 0xF0,                         // lea rdx, [rbp - 0x10]
        0x4C, 0x8D, 0x45, 0x10,                         // lea r8, [rbp + 0x10]
        0x49, 0xB9,                                     // mov r9, vector_push_back_address
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x85, 0xC0,                                     // test eax, eax
        0x75, 0x27,                                     // jne seeded_path
        0x45, 0x39, 0xAE, 0,0,0,0,                      // cmp dword ptr [r14 + subleague_count_offset], r13d
        0x7E, 0x0C,                                     // jle no_subleague
        0x49, 0x8B, 0x86, 0,0,0,0,                      // mov rax, qword ptr [r14 + subleague_array_offset]
        0x4C, 0x8B, 0x30,                               // mov r14, qword ptr [rax]
        0xEB, 0x03,                                     // jmp fallback_done
        0x4D, 0x8B, 0xF5,                               // no_subleague: mov r14, r13
        0x45, 0x8B, 0xFD,                               // fallback_done: mov r15d, r13d
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0xB8,                                     // seeded_path: mov rax, seeded_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[17], (uint64_t)(uintptr_t)vector_push_back_address);
    write_u64(&code[27], (uint64_t)(uintptr_t)&ootp_kbo_seed_single_division_allstar_candidate_teams);
    write_u32(&code[48], subleague_count_offset);
    write_u32(&code[57], subleague_array_offset);
    write_u64(&code[74], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[86], (uint64_t)(uintptr_t)seeded_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
