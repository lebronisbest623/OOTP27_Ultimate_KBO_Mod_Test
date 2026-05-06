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
