static int patch_kbo_no_minor_contract_write_site(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t size)
{
    uint8_t* target = (uint8_t*)((uintptr_t)exe + rva);
    if (!memory_range_readable(target, size)) {
        append_logf("%s target unreadable target=%p rva=0x%x", label, target, rva);
        return 0;
    }
    return patch_static_bytes(label, target, expected, patch, size);
}

static int patch_kbo_no_minor_contract_scan_byte(uint8_t* imm_ptr)
{
    if (imm_ptr == NULL || !memory_range_readable(imm_ptr, 1)) {
        return 0;
    }
    if (*imm_ptr == 1u) {
        return 0;
    }
    if (*imm_ptr != 0u && *imm_ptr != 2u && *imm_ptr != 3u) {
        return 0;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(imm_ptr, 1, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("KBO no-minor-contract scan patch VirtualProtect failed target=%p error=%lu", imm_ptr, GetLastError());
        return 0;
    }

    uint8_t old_value = *imm_ptr;
    *imm_ptr = 1u;
    FlushInstructionCache(GetCurrentProcess(), imm_ptr, 1);

    DWORD ignored = 0;
    VirtualProtect(imm_ptr, 1, old_protect, &ignored);
    append_logf(
        "installed KBO no-minor-contract scan patch imm target=%p old=%u new=1",
        imm_ptr,
        (unsigned)old_value);
    return 1;
}

static int install_kbo_no_minor_contract_scan_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        append_log_line("KBO no-minor-contract scan patch skipped: invalid DOS header");
        return 0;
    }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        append_log_line("KBO no-minor-contract scan patch skipped: invalid NT header");
        return 0;
    }

    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    int patched = 0;
    int scanned_sites = 0;
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }
        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD size = sections[s].Misc.VirtualSize;
        if (size < 12u || !memory_range_readable(section, size)) {
            continue;
        }
        for (DWORD off = 0; off + 8u < size; off++) {
            uint8_t* p = section + off;
            if (p[0] != 0xC6u) {
                continue;
            }
            /* C6 /0 disp32 imm8: mov byte ptr [reg+0x8a8], imm8 */
            if (p[2] == 0xA8u && p[3] == 0x08u && p[4] == 0x00u && p[5] == 0x00u) {
                uint8_t value = p[6];
                if (value == 0u || value == 2u || value == 3u) {
                    scanned_sites++;
                    patched += patch_kbo_no_minor_contract_scan_byte(p + 6);
                }
            }
            /* C6 /0 SIB disp32 imm8: mov byte ptr [base+index+0x8a8], imm8 */
            if (p[3] == 0xA8u && p[4] == 0x08u && p[5] == 0x00u && p[6] == 0x00u) {
                uint8_t value = p[7];
                if (value == 0u || value == 2u || value == 3u) {
                    scanned_sites++;
                    patched += patch_kbo_no_minor_contract_scan_byte(p + 7);
                }
            }
        }
    }

    append_logf(
        "KBO no-minor-contract scan patch complete scanned_sites=%d patched=%d",
        scanned_sites,
        patched);
    return patched > 0;
}

static int patch_kbo_no_minor_contract_string_at(uint8_t* target, const char* from, const char* to, size_t len)
{
    if (target == NULL || from == NULL || to == NULL || len == 0 || !memory_range_readable(target, len)) {
        return 0;
    }
    if (memcmp(target, from, len) != 0) {
        return 0;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, len, PAGE_READWRITE, &old_protect)) {
        append_logf("KBO no-minor-contract string patch VirtualProtect failed target=%p error=%lu", target, GetLastError());
        return 0;
    }

    memcpy(target, to, len);
    FlushInstructionCache(GetCurrentProcess(), target, len);

    DWORD ignored = 0;
    VirtualProtect(target, len, old_protect, &ignored);
    append_logf("installed KBO no-minor-contract string patch target=%p text=%.*s", target, (int)len, to);
    return 1;
}

static int install_kbo_no_minor_contract_string_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const char from_title[] = "Minor League Contract";
    const char to_title[] = "Major League Contract";
    const char from_sentence[] = "Minor League contract";
    const char to_sentence[] = "Major League contract";
    const char from_lower[] = "minor league contract";
    const char to_lower[] = "major league contract";
    const char from_option[] = "Minor League with Major League Option";
    const char to_option[] = "Major League Contract                ";
    const size_t max_patch_len = sizeof(from_option) - 1u;

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        append_log_line("KBO no-minor-contract string patch skipped: invalid DOS header");
        return 0;
    }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        append_log_line("KBO no-minor-contract string patch skipped: invalid NT header");
        return 0;
    }

    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    int patched = 0;
    int scanned = 0;
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_READ)) {
            continue;
        }
        if (chars & IMAGE_SCN_MEM_EXECUTE) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD size = sections[s].Misc.VirtualSize;
        if (size < max_patch_len || !memory_range_readable(section, size)) {
            continue;
        }

        for (DWORD off = 0; off + max_patch_len < size; off++) {
            uint8_t* p = section + off;
            if (memcmp(p, from_title, sizeof(from_title) - 1u) == 0) {
                scanned++;
                patched += patch_kbo_no_minor_contract_string_at(
                    p,
                    from_title,
                    to_title,
                    sizeof(from_title) - 1u);
            } else if (memcmp(p, from_sentence, sizeof(from_sentence) - 1u) == 0) {
                scanned++;
                patched += patch_kbo_no_minor_contract_string_at(
                    p,
                    from_sentence,
                    to_sentence,
                    sizeof(from_sentence) - 1u);
            } else if (memcmp(p, from_lower, sizeof(from_lower) - 1u) == 0) {
                scanned++;
                patched += patch_kbo_no_minor_contract_string_at(
                    p,
                    from_lower,
                    to_lower,
                    sizeof(from_lower) - 1u);
            } else if (memcmp(p, from_option, sizeof(from_option) - 1u) == 0) {
                scanned++;
                patched += patch_kbo_no_minor_contract_string_at(
                    p,
                    from_option,
                    to_option,
                    sizeof(from_option) - 1u);
            }
        }
    }

    append_logf(
        "KBO no-minor-contract string patch complete scanned_sites=%d patched=%d",
        scanned,
        patched);
    return patched > 0;
}

static int install_kbo_no_minor_contract_offer_callback_probe_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_FA_OFFER_SCREEN_CALLBACK_PROBE_RVA);
    const size_t stolen_len = 20;
    const uint8_t expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,                   /* mov [rsp+0x8],rbx */
        0x48, 0x89, 0x6C, 0x24, 0x10,                   /* mov [rsp+0x10],rbp */
        0x48, 0x89, 0x74, 0x24, 0x20,                   /* mov [rsp+0x20],rsi */
        0x57,                                           /* push rdi */
        0x48, 0x83, 0xEC, 0x20                          /* sub rsp,0x20 */
    };

    if (!memory_range_readable(target, stolen_len)) {
        append_logf("KBO no-minor offer callback probe target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor offer callback probe already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor offer callback probe", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO no-minor offer callback probe trampoline");
        return 0;
    }
    uint8_t* stub = build_kbo_fa_offer_screen_callback_probe_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor offer callback probe detour stub");
        return 0;
    }

    uint8_t patch[20] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor offer callback probe error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO no-minor offer callback probe target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_fa_offer_screen_callback_probe_wrapper);
    return 1;
}

static int install_kbo_no_minor_contract_contract_callback_probe_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_FA_CONTRACT_OFFER_CALLBACK_PROBE_RVA);
    const size_t stolen_len = 21;
    const uint8_t expected[21] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,                   /* mov [rsp+0x8],rbx */
        0x48, 0x89, 0x74, 0x24, 0x10,                   /* mov [rsp+0x10],rsi */
        0x55,                                           /* push rbp */
        0x57,                                           /* push rdi */
        0x41, 0x54,                                     /* push r12 */
        0x41, 0x56,                                     /* push r14 */
        0x41, 0x57,                                     /* push r15 */
        0x48, 0x8B, 0xEC                                /* mov rbp,rsp */
    };

    if (!memory_range_readable(target, stolen_len)) {
        append_logf("KBO no-minor contract callback probe target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor contract callback probe already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor contract callback probe", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO no-minor contract callback probe trampoline");
        return 0;
    }
    uint8_t* stub = build_kbo_fa_contract_offer_callback_probe_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor contract callback probe detour stub");
        return 0;
    }

    uint8_t patch[21] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor contract callback probe error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO no-minor contract callback probe target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_fa_contract_offer_callback_probe_wrapper);
    return 1;
}

static int install_kbo_no_minor_contract_player_action_eligibility_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_PLAYER_ACTION_ELIGIBILITY_RVA);
    const size_t stolen_len = 21;
    const uint8_t expected[21] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,                   /* mov [rsp+0x8],rbx */
        0x44, 0x88, 0x44, 0x24, 0x18,                   /* mov [rsp+0x18],r8b */
        0x55,                                           /* push rbp */
        0x56,                                           /* push rsi */
        0x57,                                           /* push rdi */
        0x41, 0x54,                                     /* push r12 */
        0x41, 0x55,                                     /* push r13 */
        0x41, 0x56,                                     /* push r14 */
        0x41, 0x57                                      /* push r15 */
    };

    if (!memory_range_readable(target, stolen_len)) {
        append_logf("KBO no-minor player action eligibility target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor player action eligibility already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor player action eligibility", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO no-minor player action eligibility trampoline");
        return 0;
    }
    uint8_t* stub = build_kbo_player_action_eligibility_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor player action eligibility detour stub");
        return 0;
    }

    uint8_t patch[21] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor player action eligibility error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO no-minor player action eligibility target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_player_action_eligibility_wrapper);
    return 1;
}

static int install_kbo_no_minor_contract_offer_player_demand_floor_patch(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    size_t stolen_len,
    int alt_path)
{
    if (exe == NULL || label == NULL || expected == NULL || stolen_len < 12 || stolen_len > 24) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + rva);
    if (!memory_range_readable(target, stolen_len)) {
        append_logf("%s target unreadable target=%p", label, target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, stolen_len) != 0) {
        log_patch_bytes_mismatch(label, target, stolen_len);
        return 0;
    }

    uint8_t* stub = alt_path
        ? build_kbo_fa_offer_player_demand_floor_17b50b4_stub(target + stolen_len)
        : build_kbo_fa_offer_player_demand_floor_17a79bb_stub(target + stolen_len);
    if (stub == NULL) {
        append_logf("failed to allocate %s detour stub", label);
        return 0;
    }

    uint8_t patch[24];
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0x48;                                    /* mov rax, stub */
    patch[1] = 0xB8;
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);
    patch[10] = 0xFF;                                   /* jmp rax */
    patch[11] = 0xE0;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, stolen_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, stolen_len);
    FlushInstructionCache(GetCurrentProcess(), target, stolen_len);

    DWORD ignored = 0;
    VirtualProtect(target, stolen_len, old_protect, &ignored);

    append_logf(
        "installed %s target=%p stub=%p probe=%p",
        label,
        target,
        stub,
        &ootp_kbo_fa_offer_player_demand_floor_probe);
    return 1;
}

static int install_kbo_no_minor_contract_demand_write_floor_aab739_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_RVA);
    const uint8_t expected[13] = {
        0x89, 0x83, 0x84, 0x0D, 0x00, 0x00,             /* mov [rbx+0xd84],eax */
        0xE8, 0xFC, 0xF2, 0x20, 0x01,                   /* call 0x141cbaa40 */
        0xEB, 0x4E                                      /* jmp 0x140aab794 */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO no-minor demand write floor AAB739 target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor demand write floor AAB739 already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor demand write floor AAB739", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_demand_write_floor_aab739_stub(
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_NOTIFY_RVA),
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_DONE_RVA));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor demand write floor AAB739 detour stub");
        return 0;
    }

    uint8_t patch[13] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor demand write floor AAB739 error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf("installed KBO no-minor demand write floor AAB739 target=%p stub=%p", target, stub);
    return 1;
}

static int install_kbo_no_minor_contract_demand_write_floor_1077952_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_1077952_RVA);
    const uint8_t expected[13] = {
        0x89, 0x81, 0x84, 0x0D, 0x00, 0x00,             /* mov [rcx+0xd84],eax */
        0x41, 0x8B, 0x97, 0x20, 0x01, 0x00, 0x00        /* mov edx,[r15+0x120] */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO no-minor demand write floor 1077952 target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor demand write floor 1077952 already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor demand write floor 1077952", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_demand_write_floor_1077952_stub(target + sizeof(expected));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor demand write floor 1077952 detour stub");
        return 0;
    }

    uint8_t patch[13] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor demand write floor 1077952 error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf("installed KBO no-minor demand write floor 1077952 target=%p stub=%p", target, stub);
    return 1;
}

static int install_kbo_foreign_fa_demand_baseline_prepare_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_FOREIGN_FA_DEMAND_BASELINE_PREPARE_AAB624_RVA);
    const uint8_t expected[14] = {
        0x41, 0x8B, 0xBD, 0x74, 0x03, 0x00, 0x00,       /* mov edi,[r13+0x374] */
        0x0F, 0xB6, 0xC0,                               /* movzx eax,al */
        0x66, 0x0F, 0x6E, 0xCF                          /* movd xmm1,edi */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO foreign FA demand baseline prepare target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO foreign FA demand baseline prepare already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO foreign FA demand baseline prepare", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_foreign_fa_demand_baseline_prepare_aab624_stub(target + sizeof(expected));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO foreign FA demand baseline prepare detour stub");
        return 0;
    }

    uint8_t patch[14] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO foreign FA demand baseline prepare error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf("installed KBO foreign FA demand baseline prepare target=%p stub=%p", target, stub);
    return 1;
}

static int install_kbo_no_minor_contract_submit_salary_floor_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_SUBMIT_SALARY_FLOOR_13B0712_RVA);
    const uint8_t expected[26] = {
        0x38, 0x9F, 0xCC, 0x00, 0x00, 0x00,             /* cmp [rdi+0xcc], bl */
        0x0F, 0x84, 0xDD, 0x01, 0x00, 0x00,             /* je 0x1413b08fb */
        0x8B, 0x87, 0xC0, 0x00, 0x00, 0x00,             /* mov eax, [rdi+0xc0] */
        0x85, 0xC0,                                     /* test eax, eax */
        0x0F, 0x85, 0x8F, 0x00, 0x00, 0x00              /* jne 0x1413b07bb */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO no-minor submit salary floor target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO no-minor submit salary floor already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor submit salary floor", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_contract_submit_salary_floor_stub(
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_SUBMIT_ZERO_ERROR_13B072C_RVA),
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_SUBMIT_MIN_CHECK_13B07BB_RVA),
        (void*)((uintptr_t)exe + OOTP27_NO_MINOR_CONTRACT_SUBMIT_AFTER_MIN_CHECK_13B0860_RVA));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO no-minor submit salary floor detour stub");
        return 0;
    }

    uint8_t patch[26] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO no-minor submit salary floor error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO no-minor submit salary floor target=%p stub=%p floor_source=[r14+0x278]",
        target,
        stub);
    return 1;
}

static int install_kbo_no_minor_contract_experimental_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO no-minor-contract experimental patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO no-minor-contract experimental patch host=%s", host);
        return 0;
    }

    InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 1);

    const uint8_t expected_rax[7] = {0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x00};
    const uint8_t patch_rax[7]    = {0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x01};
    const uint8_t expected_rax_option[7] = {0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x02};
    const uint8_t expected_rax_option_alt[7] = {0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x03};
    const uint8_t expected_rdi[7] = {0xC6, 0x87, 0xA8, 0x08, 0x00, 0x00, 0x00};
    const uint8_t patch_rdi[7]    = {0xC6, 0x87, 0xA8, 0x08, 0x00, 0x00, 0x01};
    const uint8_t expected_fa_offer_demand_floor[14] = {
        0x66, 0x89, 0x75, 0x98,                         /* mov [rbp-0x68],si */
        0x49, 0x89, 0xB6, 0xD8, 0x00, 0x00, 0x00,       /* mov [r14+0xd8],rsi */
        0x8D, 0x4E, 0x04                                /* lea ecx,[rsi+4] */
    };
    const uint8_t expected_fa_offer_alt_demand_floor[20] = {
        0x45, 0x38, 0xBE, 0xC3, 0x0D, 0x00, 0x00,       /* cmp r15b,[r14+0xdc3] */
        0x74, 0x0B,                                     /* je +0xb */
        0x41, 0xC7, 0x85, 0x1C, 0x03, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00                          /* mov [r13+0x31c],8 */
    };

    int ok = 0;
    ok |= install_kbo_no_minor_contract_offer_callback_probe_patch(exe);
    ok |= install_kbo_no_minor_contract_contract_callback_probe_patch(exe);
    ok |= install_kbo_no_minor_contract_player_action_eligibility_patch(exe);
    ok |= install_kbo_no_minor_contract_offer_player_demand_floor_patch(
        exe,
        "KBO no-minor demand floor FA offer build 17A79BB",
        OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA,
        expected_fa_offer_demand_floor,
        sizeof(expected_fa_offer_demand_floor),
        0);
    ok |= install_kbo_no_minor_contract_offer_player_demand_floor_patch(
        exe,
        "KBO no-minor demand floor FA offer alt build 17B50B4",
        OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA,
        expected_fa_offer_alt_demand_floor,
        sizeof(expected_fa_offer_alt_demand_floor),
        1);
    ok |= install_kbo_no_minor_contract_demand_write_floor_aab739_patch(exe);
    ok |= install_kbo_no_minor_contract_demand_write_floor_1077952_patch(exe);
    ok |= install_kbo_foreign_fa_demand_baseline_prepare_patch(exe);
    ok |= install_kbo_no_minor_contract_submit_salary_floor_patch(exe);
    const uint8_t expected_dynamic_zero_branch[2] = {0x32, 0xC0};
    const uint8_t patch_dynamic_zero_branch[2] = {0xB0, 0x01};
    const uint8_t expected_dynamic_reset[7] = {0x41, 0x88, 0xB7, 0xA8, 0x08, 0x00, 0x00};
    const uint8_t patch_dynamic_reset[7] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    const uint8_t expected_offer_major_flag_copy[8] = {0x88, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x84, 0xC0};
    const uint8_t patch_offer_major_flag_copy[8] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x01, 0x90};
    const uint8_t expected_offer_major_flag_reset[7] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x00};
    const uint8_t patch_offer_major_flag_reset[7] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x01};
    const uint8_t expected_option_text_branch[2] = {0x75, 0x07};
    const uint8_t patch_option_text_branch[2] = {0x90, 0x90};
    const uint8_t expected_offer_ui_minor_item[3] = {0x45, 0x33, 0xC9};
    const uint8_t patch_offer_ui_minor_item[3] = {0xEB, 0x22, 0x90};
    const uint8_t expected_offer_ui_option_item[3] = {0x45, 0x33, 0xC9};
    const uint8_t patch_offer_ui_option_item[3] = {0xEB, 0x28, 0x90};
    const uint8_t expected_offer_ui_selection_value[5] = {0xBA, 0x02, 0x00, 0x00, 0x00};
    const uint8_t patch_offer_ui_selection_value[5] = {0xBA, 0x01, 0x00, 0x00, 0x00};
    const uint8_t expected_offer_ui_selection_branch[2] = {0x75, 0x04};
    const uint8_t patch_offer_ui_selection_branch[2] = {0xEB, 0x04};
    const uint8_t expected_offer_report_minor_item[5] = {0x45, 0x33, 0xC9, 0x4C, 0x8D};
    const uint8_t patch_offer_report_minor_item[5] = {0xE9, 0xD6, 0x3A, 0x00, 0x00};
    const uint8_t expected_offer_report_option_item[5] = {0x45, 0x33, 0xC9, 0x4C, 0x8D};
    const uint8_t patch_offer_report_option_item[5] = {0xE9, 0xBA, 0x00, 0x00, 0x00};
    const uint8_t expected_contract_type_minor_item[5] = {0x45, 0x33, 0xC0, 0x48, 0x8D};
    const uint8_t patch_contract_type_minor_item[5] = {0xE9, 0x1D, 0x00, 0x00, 0x00};
    const uint8_t expected_minor_action_case[9] = {0x83, 0xFF, 0x35, 0x0F, 0x85, 0x33, 0x01, 0x00, 0x00};
    const uint8_t patch_minor_action_case[9] = {0xE9, 0x37, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90};
    const uint8_t expected_minor_extension_action_case[9] = {0x83, 0xFF, 0x36, 0x0F, 0x85, 0x43, 0x01, 0x00, 0x00};
    const uint8_t patch_minor_extension_action_case[9] = {0xE9, 0x47, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90};
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract dynamic zero branch 6331F5",
        OOTP27_NO_MINOR_CONTRACT_DYNAMIC_ZERO_6331F5_RVA,
        expected_dynamic_zero_branch,
        patch_dynamic_zero_branch,
        sizeof(expected_dynamic_zero_branch));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract dynamic reset 633286",
        OOTP27_NO_MINOR_CONTRACT_DYNAMIC_RESET_633286_RVA,
        expected_dynamic_reset,
        patch_dynamic_reset,
        sizeof(expected_dynamic_reset));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract offer major flag copy 13B0235",
        OOTP27_NO_MINOR_CONTRACT_OFFER_MAJOR_FLAG_COPY_13B0235_RVA,
        expected_offer_major_flag_copy,
        patch_offer_major_flag_copy,
        sizeof(expected_offer_major_flag_copy));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract offer major flag reset 13B03CE",
        OOTP27_NO_MINOR_CONTRACT_OFFER_MAJOR_FLAG_RESET_13B03CE_RVA,
        expected_offer_major_flag_reset,
        patch_offer_major_flag_reset,
        sizeof(expected_offer_major_flag_reset));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract option text branch 13E800B",
        OOTP27_NO_MINOR_CONTRACT_OPTION_TEXT_BRANCH_13E800B_RVA,
        expected_option_text_branch,
        patch_option_text_branch,
        sizeof(expected_option_text_branch));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract offer UI skip minor item 17A93DF",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_MINOR_ITEM_17A93DF_RVA,
        expected_offer_ui_minor_item,
        patch_offer_ui_minor_item,
        sizeof(expected_offer_ui_minor_item));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract offer UI skip option item 17A9467",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_OPTION_ITEM_17A9467_RVA,
        expected_offer_ui_option_item,
        patch_offer_ui_option_item,
        sizeof(expected_offer_ui_option_item));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract offer UI force selected major 17A94A0",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_SELECTION_VALUE_17A94A0_RVA,
        expected_offer_ui_selection_value,
        patch_offer_ui_selection_value,
        sizeof(expected_offer_ui_selection_value));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract offer UI force selection branch 17A94A5",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_SELECTION_BRANCH_17A94A5_RVA,
        expected_offer_ui_selection_branch,
        patch_offer_ui_selection_branch,
        sizeof(expected_offer_ui_selection_branch));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract report skip minor item 17DA0EC",
        OOTP27_NO_MINOR_CONTRACT_OFFER_REPORT_MINOR_ITEM_17DA0EC_RVA,
        expected_offer_report_minor_item,
        patch_offer_report_minor_item,
        sizeof(expected_offer_report_minor_item));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract report skip option item 17DA2C6",
        OOTP27_NO_MINOR_CONTRACT_OFFER_REPORT_OPTION_ITEM_17DA2C6_RVA,
        expected_offer_report_option_item,
        patch_offer_report_option_item,
        sizeof(expected_offer_report_option_item));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract contract type skip minor item 182666A",
        OOTP27_NO_MINOR_CONTRACT_CONTRACT_TYPE_MINOR_ITEM_182666A_RVA,
        expected_contract_type_minor_item,
        patch_contract_type_minor_item,
        sizeof(expected_contract_type_minor_item));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract block minor action case 1072DF6",
        OOTP27_NO_MINOR_CONTRACT_MINOR_ACTION_CASE_1072DF6_RVA,
        expected_minor_action_case,
        patch_minor_action_case,
        sizeof(expected_minor_action_case));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract block minor extension action case 1072F32",
        OOTP27_NO_MINOR_CONTRACT_MINOR_EXTENSION_ACTION_CASE_1072F32_RVA,
        expected_minor_extension_action_case,
        patch_minor_extension_action_case,
        sizeof(expected_minor_extension_action_case));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch write 638D32",
        OOTP27_NO_MINOR_CONTRACT_WRITE_638D32_RVA,
        expected_rax,
        patch_rax,
        sizeof(expected_rax));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch option write 6393A3",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6393A3_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch option write 6396D4",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6396D4_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch write 6DE7C1",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE7C1_RVA,
        expected_rax,
        patch_rax,
        sizeof(expected_rax));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch option write 6DDEA2",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DDEA2_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch option write 6DE33E",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE33E_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch option write 6DE58C",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE58C_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch option write 6DE6BF",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE6BF_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch option-alt write 6DE96E",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE96E_RVA,
        expected_rax_option_alt,
        patch_rax,
        sizeof(expected_rax_option_alt));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch write 5568EF",
        OOTP27_NO_MINOR_CONTRACT_WRITE_5568EF_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch write A4BE04",
        OOTP27_NO_MINOR_CONTRACT_WRITE_A4BE04_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch write 2184B61",
        OOTP27_NO_MINOR_CONTRACT_WRITE_2184B61_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch write 2185963",
        OOTP27_NO_MINOR_CONTRACT_WRITE_2185963_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi));
    ok |= patch_kbo_no_minor_contract_write_site(
        exe,
        "KBO no-minor-contract experimental patch write 2185D00",
        OOTP27_NO_MINOR_CONTRACT_WRITE_2185D00_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi));
    ok |= install_kbo_no_minor_contract_scan_patch(exe);
    ok |= install_kbo_no_minor_contract_string_patch(exe);

    append_logf("KBO no-minor-contract experimental patch complete installed_any=%d", ok);
    return ok;
}
