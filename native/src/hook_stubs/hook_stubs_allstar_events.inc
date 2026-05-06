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

static uint8_t* build_allstar_schedule_import_capture_stub(void* return_address, uint32_t game_flag_offset)
{
    uint8_t code[] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x49, 0x8B, 0xCD,                               // mov rcx, r13
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
        0x41, 0xC6, 0x85, 0,0,0,0, 0x01,                // mov byte ptr [r13 + game_flag_offset], 1
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[20], (uint64_t)(uintptr_t)&ootp_kbo_capture_allstar_schedule_import_league);
    write_u32(&code[48], game_flag_offset);
    write_u64(&code[55], (uint64_t)(uintptr_t)return_address);

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
