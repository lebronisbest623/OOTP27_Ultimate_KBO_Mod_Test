static uint8_t* build_kbo_season_phase_write_probe_call_stub(
    void* patch_site,
    uint8_t base_reg,
    uint32_t site_rva,
    uint8_t value)
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
        0x48, 0x89, 0xD9,                               // mov rcx, rbx placeholder
        0xBA, 0,0,0,0,                                  // mov edx, value
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, site_rva
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
        0xC3,                                           // ret
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC
    };

    switch (base_reg) {
    case 1: code[16] = 0x48; code[17] = 0x89; code[18] = 0xC9; break; /* rcx */
    case 2: code[16] = 0x48; code[17] = 0x89; code[18] = 0xD1; break; /* rdx */
    case 3: code[16] = 0x48; code[17] = 0x89; code[18] = 0xD9; break; /* rbx */
    case 6: code[16] = 0x48; code[17] = 0x89; code[18] = 0xF1; break; /* rsi */
    case 7: code[16] = 0x48; code[17] = 0x89; code[18] = 0xF9; break; /* rdi */
    case 14: code[16] = 0x4C; code[17] = 0x89; code[18] = 0xF1; break; /* r14 */
    case 15: code[16] = 0x4C; code[17] = 0x89; code[18] = 0xF9; break; /* r15 */
    default: return NULL;
    }

    write_u32(&code[20], (uint32_t)value);
    write_u32(&code[26], site_rva);
    write_u64(&code[32], (uint64_t)(uintptr_t)&ootp_kbo_season_phase_write_probe);

    uint8_t* memory = kbo_alloc_near_code(patch_site, sizeof(code));
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
