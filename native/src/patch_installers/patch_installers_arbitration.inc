static int patch_kbo_salary_arbitration_bytes(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t size)
{
    if (exe == NULL || label == NULL || expected == NULL || patch == NULL || size == 0) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + rva);
    if (!memory_range_readable(target, size)) {
        append_logf("%s target unreadable target=%p rva=0x%x", label, target, rva);
        return 0;
    }

    return patch_static_bytes(label, target, expected, patch, size);
}

static uint8_t* allocate_kbo_salary_arbitration_stub(const uint8_t* code, size_t size)
{
    if (code == NULL || size == 0) {
        return NULL;
    }

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }

    memcpy(memory, code, size);
    FlushInstructionCache(GetCurrentProcess(), memory, size);
    return memory;
}

static uint8_t* build_kbo_salary_arbitration_final_zero_tender_stub(
    void* non_tender_continuation,
    void* tender_continuation)
{
    uint8_t code[112] = {
        0x41, 0x83, 0xBD, 0x7C, 0x08, 0x00, 0x00, 0x00, /* cmp dword ptr [r13+0x87c], 0 */
        0x0F, 0x8F, 0x42, 0x00, 0x00, 0x00,             /* jg tender_continuation */
        0x41, 0x83, 0x7D, 0x48, 0x64,                   /* cmp dword ptr [r13+0x48], 100 */
        0x74, 0x21,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBE, 0x20, 0x01, 0x00, 0x00, 0x64, /* cmp dword ptr [r14+0x120], 100 */
        0x74, 0x17,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBD, 0x70, 0x0D, 0x00, 0x00, 0x64, /* cmp dword ptr [r13+0xd70], 100 */
        0x74, 0x0D,                                     /* je clamp_to_floor */
        0x49, 0xBB,                                     /* mov r11, non_tender_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0x45, 0x8B, 0x9C, 0x24, 0x78, 0x02, 0x00, 0x00, /* mov r11d, [r12+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg write_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x45, 0x89, 0x9D, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], r11d */
        0x49, 0xBB,                                     /* mov r11, tender_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[43], (uint64_t)(uintptr_t)non_tender_continuation);
    write_u64(&code[82], (uint64_t)(uintptr_t)tender_continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

static uint8_t* build_kbo_salary_arbitration_ai_offer_write_681012_stub(
    void* offer_above_strong_continuation,
    void* offer_not_above_strong_continuation)
{
    uint8_t code[96] = {
        0x41, 0x83, 0x7D, 0x48, 0x64,                   /* cmp dword ptr [r13+0x48], 100 */
        0x75, 0x1B,                                     /* jne write_original */
        0x45, 0x8B, 0x9C, 0x24, 0x78, 0x02, 0x00, 0x00, /* mov r11d, [r12+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg compare_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x41, 0x3B, 0xC3,                               /* cmp eax, r11d */
        0x7D, 0x03,                                     /* jge write_original */
        0x41, 0x8B, 0xC3,                               /* mov eax, r11d */
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], eax */
        0x66, 0x41, 0x0F, 0x2F, 0xF3,                   /* comisd xmm6, xmm11 */
        0x76, 0x0D,                                     /* jbe offer_not_above_strong_continuation */
        0x49, 0xBB,                                     /* mov r11, offer_above_strong_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0x49, 0xBB,                                     /* mov r11, offer_not_above_strong_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[50], (uint64_t)(uintptr_t)offer_above_strong_continuation);
    write_u64(&code[63], (uint64_t)(uintptr_t)offer_not_above_strong_continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

static uint8_t* build_kbo_salary_arbitration_ai_offer_write_6827cd_stub(
    void* original_lea_value,
    void* continuation)
{
    uint8_t code[80] = {
        0x83, 0x7B, 0x48, 0x64,                         /* cmp dword ptr [rbx+0x48], 100 */
        0x75, 0x1A,                                     /* jne write_original */
        0x45, 0x8B, 0x9F, 0x78, 0x02, 0x00, 0x00,       /* mov r11d, [r15+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg compare_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x41, 0x3B, 0xC3,                               /* cmp eax, r11d */
        0x7D, 0x03,                                     /* jge write_original */
        0x41, 0x8B, 0xC3,                               /* mov eax, r11d */
        0x89, 0x83, 0x7C, 0x08, 0x00, 0x00,             /* mov [rbx+0x87c], eax */
        0x48, 0xB8,                                     /* mov rax, original_lea_value */
        0,0,0,0,0,0,0,0,
        0x49, 0xBB,                                     /* mov r11, continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[40], (uint64_t)(uintptr_t)original_lea_value);
    write_u64(&code[50], (uint64_t)(uintptr_t)continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

static uint8_t* build_kbo_salary_arbitration_zero_offer_check_682089_stub(
    void* non_tender_continuation,
    void* continuation)
{
    uint8_t code[112] = {
        0x41, 0x8B, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov eax, [r13+0x87c] */
        0x85, 0xC0,                                     /* test eax, eax */
        0x75, 0x38,                                     /* jne normal */
        0x41, 0x83, 0x7D, 0x48, 0x64,                   /* cmp dword ptr [r13+0x48], 100 */
        0x74, 0x14,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBE, 0x20, 0x01, 0x00, 0x00, 0x64, /* cmp dword ptr [r14+0x120], 100 */
        0x74, 0x0A,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBD, 0x70, 0x0D, 0x00, 0x00, 0x64, /* cmp dword ptr [r13+0xd70], 100 */
        0x75, 0x31,                                     /* jne non_tender_continuation */
        0x45, 0x8B, 0x9C, 0x24, 0x78, 0x02, 0x00, 0x00, /* mov r11d, [r12+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg write_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x44, 0x89, 0xD8,                               /* mov eax, r11d */
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], eax */
        0x41, 0x8B, 0x8D, 0x80, 0x08, 0x00, 0x00,       /* mov ecx, [r13+0x880] */
        0x49, 0xBB,                                     /* mov r11, continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0x49, 0xBB,                                     /* mov r11, non_tender_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[76], (uint64_t)(uintptr_t)continuation);
    write_u64(&code[89], (uint64_t)(uintptr_t)non_tender_continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

static uint8_t* build_kbo_salary_arbitration_high_limit_non_tender_6820af_stub(
    void* tender_continuation,
    void* non_tender_call_continuation)
{
    uint8_t code[120] = {
        0x48, 0x8B, 0x75, 0xB8,                         /* mov rsi, [rbp-0x48] */
        0x4C, 0x8B, 0x75, 0xC0,                         /* mov r14, [rbp-0x40] */
        0x41, 0x83, 0x7D, 0x48, 0x64,                   /* cmp dword ptr [r13+0x48], 100 */
        0x74, 0x14,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBE, 0x20, 0x01, 0x00, 0x00, 0x64, /* cmp dword ptr [r14+0x120], 100 */
        0x74, 0x0A,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBD, 0x70, 0x0D, 0x00, 0x00, 0x64, /* cmp dword ptr [r13+0xd70], 100 */
        0x75, 0x35,                                     /* jne non_tender_call */
        0x49, 0x8D, 0xBD, 0x14, 0x09, 0x00, 0x00,       /* lea rdi, [r13+0x914] */
        0x49, 0x8D, 0x9D, 0xA0, 0x08, 0x00, 0x00,       /* lea rbx, [r13+0x8a0] */
        0x45, 0x8B, 0x9C, 0x24, 0x78, 0x02, 0x00, 0x00, /* mov r11d, [r12+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg write_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x45, 0x89, 0x9D, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], r11d */
        0x49, 0xBB,                                     /* mov r11, tender_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0x45, 0x0F, 0xB6, 0xC7,                         /* movzx r8d, r15b */
        0x49, 0x8B, 0xD5,                               /* mov rdx, r13 */
        0x49, 0x8B, 0xCE,                               /* mov rcx, r14 */
        0x49, 0xBB,                                     /* mov r11, non_tender_call_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC
    };

    write_u64(&code[77], (uint64_t)(uintptr_t)tender_continuation);
    write_u64(&code[100], (uint64_t)(uintptr_t)non_tender_call_continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

typedef void (__fastcall *OotpArbitrationNonTenderFn)(void* team, void* player, uint8_t notify);
typedef uint8_t* (__fastcall *KboArbitrationLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);

static int32_t kbo_salary_arbitration_resolve_minimum_salary(uint32_t league_id)
{
    if (league_id == 0u || league_id > 100000u) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    uintptr_t global_db = get_ootp_global_database();
    HMODULE exe = GetModuleHandleA(NULL);
    if (global_db == 0 || exe == NULL) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    KboArbitrationLeagueFinancialsLookupFn lookup =
        (KboArbitrationLeagueFinancialsLookupFn)((uintptr_t)exe + OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    uint8_t* financials = lookup((void*)global_db, (int32_t)league_id);
    if (financials == NULL
            || !memory_range_readable(financials + OOTP27_LEAGUE_MINIMUM_SALARY_OFFSET, sizeof(int32_t))) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    int32_t minimum_salary = *(int32_t*)(financials + OOTP27_LEAGUE_MINIMUM_SALARY_OFFSET);
    if (minimum_salary <= 0 || minimum_salary > 1000000000) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }
    return minimum_salary;
}

static uintptr_t kbo_salary_arbitration_caller_rva(void* return_address)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL || return_address == NULL) {
        return 0;
    }
    return (uintptr_t)return_address - (uintptr_t)exe;
}

static int kbo_salary_arbitration_is_known_non_tender_return(uintptr_t caller_rva)
{
    return caller_rva == 0x0067C0F1u
        || caller_rva == 0x00681B6Bu
        || caller_rva == 0x006820C6u;
}

__declspec(noinline) void ootp_kbo_salary_arbitration_non_tender_wrapper(
    void* team_ptr,
    void* player_ptr,
    uint8_t notify,
    OotpArbitrationNonTenderFn original_func)
{
    if (player_ptr == NULL || !memory_range_readable(player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        return;
    }

    void* return_address = __builtin_return_address(0);
    uintptr_t caller_rva = kbo_salary_arbitration_caller_rva(return_address);
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t player_draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint32_t team_league_id = 0u;
    uint32_t team_id = 0u;
    if (team_ptr != NULL
            && memory_range_readable(
                (uint8_t*)team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET,
                OOTP27_KBO_TEAM_ID_OFFSET - OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET + sizeof(uint32_t))) {
        uint8_t* team = (uint8_t*)team_ptr;
        team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    }

    int kbo_context =
        team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
        || player_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
        || player_draft_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
    if (!kbo_context) {
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        return;
    }

    uint8_t contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    int32_t old_offer = *(int32_t*)(player + 0x87cu);
    int direct_block_candidate = contract_level != 0u
        && old_offer <= 0
        && kbo_salary_arbitration_is_known_non_tender_return(caller_rva);
    int should_block = 0;
    if (!should_block) {
        static LONG pass_log_count = 0;
        LONG pass_slot = InterlockedIncrement(&pass_log_count);
        if (pass_slot <= 120) {
            append_logf(
                "KBO salary arbitration non-tender pass-through player=%u team=%u team_league=%u player_league=%u draft_league=%u contract_level=%u offer=%d notify=%u caller_rva=0x%llx direct_block_candidate=%d",
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                team_id,
                team_league_id,
                player_league_id,
                player_draft_league_id,
                (unsigned)contract_level,
                old_offer,
                (unsigned)notify,
                (unsigned long long)caller_rva,
                direct_block_candidate ? 1 : 0);
        }
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t floor_league_id = team_league_id != 0u
        ? team_league_id
        : (player_league_id != 0u ? player_league_id : player_draft_league_id);
    int32_t salary_floor = kbo_salary_arbitration_resolve_minimum_salary(floor_league_id);
    if (old_offer < salary_floor) {
        *(int32_t*)(player + 0x87cu) = salary_floor;
    }

    static LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 120) {
        append_logf(
            "KBO salary arbitration non-tender blocked player=%u team=%u team_league=%u player_league=%u draft_league=%u contract_level=%u old_offer=%d floor=%d notify=%u caller_rva=0x%llx",
            player_id,
            team_id,
            team_league_id,
            player_league_id,
            player_draft_league_id,
            (unsigned)contract_level,
            old_offer,
            salary_floor,
            (unsigned)notify,
            (unsigned long long)caller_rva);
    }
}

static uint8_t* build_kbo_salary_arbitration_non_tender_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x49, 0xB9,                                     /* mov r9, original_trampoline */
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     /* mov rax, wrapper */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_salary_arbitration_non_tender_wrapper);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

static int patch_kbo_salary_arbitration_r11_detour(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    size_t size,
    uint8_t* stub)
{
    if (exe == NULL || label == NULL || expected == NULL || size < 13 || stub == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + rva);
    if (!memory_range_readable(target, size)) {
        append_logf("%s target unreadable target=%p rva=0x%x", label, target, rva);
        return 0;
    }
    if (is_r11_absolute_jump_patch(target)) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, size) != 0) {
        log_patch_bytes_mismatch(label, target, size);
        return 0;
    }

    uint8_t patch[32] = {
        0x49, 0xBB,                                     /* mov r11, stub */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3                                /* jmp r11 */
    };
    for (size_t i = 13; i < size && i < sizeof(patch); i++) {
        patch[i] = 0x90;
    }
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);

    DWORD ignored = 0;
    VirtualProtect(target, size, old_protect, &ignored);

    append_logf("installed %s target=%p stub=%p", label, target, stub);
    return 1;
}

static int install_kbo_salary_arbitration_non_tender_function_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_ARBITRATION_NON_TENDER_FUNCTION_RVA);
    const size_t stolen_len = 18;
    const uint8_t expected[18] = {
        0x48, 0x89, 0x54, 0x24, 0x10,                   /* mov [rsp+0x10], rdx */
        0x48, 0x89, 0x4C, 0x24, 0x08,                   /* mov [rsp+0x8], rcx */
        0x55,                                           /* push rbp */
        0x53,                                           /* push rbx */
        0x56,                                           /* push rsi */
        0x57,                                           /* push rdi */
        0x41, 0x54,                                     /* push r12 */
        0x41, 0x55                                      /* push r13 */
    };

    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO salary arbitration non-tender function target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO salary arbitration non-tender function already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO salary arbitration non-tender function", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO salary arbitration non-tender trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_salary_arbitration_non_tender_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO salary arbitration non-tender detour stub");
        return 0;
    }

    uint8_t patch[18] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO salary arbitration non-tender function error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO salary arbitration non-tender function target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_salary_arbitration_non_tender_wrapper);
    return 1;
}

static int install_kbo_salary_arbitration_no_withdraw_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO salary arbitration no-withdraw patch");
        return 0;
    }

    const uint8_t expected_ui_hide_branch[6] = {
        0x0F, 0x85, 0x8D, 0x01, 0x00, 0x00              /* jne 0x1413604c5 */
    };
    const uint8_t patch_ui_hide_branch[6] = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90              /* always run OOTP's existing hide/disable path */
    };

    const uint8_t expected_action_gate[13] = {
        0x49, 0x81, 0xFD, 0x40, 0x42, 0x0F, 0x00,       /* cmp r13, 0xf4240 */
        0x0F, 0x8E, 0xB7, 0x02, 0x00, 0x00              /* jle 0x141360e1c */
    };
    const uint8_t patch_action_gate[13] = {
        0xE9, 0xBF, 0x02, 0x00, 0x00,                   /* jmp 0x141360e1c */
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };

    const uint8_t expected_final_zero_tender_gate[14] = {
        0x41, 0x83, 0xBD, 0x7C, 0x08, 0x00, 0x00, 0x00, /* cmp dword ptr [r13+0x87c], 0 */
        0x0F, 0x8F, 0xD8, 0x00, 0x00, 0x00              /* jg 0x140681c06 */
    };

    const uint8_t expected_ai_offer_write_681012[14] = {
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], eax */
        0x66, 0x41, 0x0F, 0x2F, 0xF3,                   /* comisd xmm6, xmm11 */
        0x76, 0x2A                                      /* jbe 0x14068204a */
    };

    const uint8_t expected_zero_offer_check_682089[18] = {
        0x41, 0x8B, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov eax, [r13+0x87c] */
        0x85, 0xC0,                                     /* test eax, eax */
        0x74, 0x23,                                     /* je 0x1406820b7 */
        0x41, 0x8B, 0x8D, 0x80, 0x08, 0x00, 0x00        /* mov ecx, [r13+0x880] */
    };

    const uint8_t expected_high_limit_non_tender_gate_6820af[18] = {
        0x48, 0x8B, 0x75, 0xB8,                         /* mov rsi, [rbp-0x48] */
        0x4C, 0x8B, 0x75, 0xC0,                         /* mov r14, [rbp-0x40] */
        0x45, 0x0F, 0xB6, 0xC7,                         /* movzx r8d, r15b */
        0x49, 0x8B, 0xD5,                               /* mov rdx, r13 */
        0x49, 0x8B, 0xCE                                /* mov rcx, r14 */
    };

    const uint8_t expected_ai_offer_write_6827cd[13] = {
        0x89, 0x83, 0x7C, 0x08, 0x00, 0x00,             /* mov [rbx+0x87c], eax */
        0x48, 0x8D, 0x05, 0xFE, 0x23, 0x43, 0x02        /* lea 0x142ab4bd8, rax */
    };

    int ok = 0;
    ok |= patch_kbo_salary_arbitration_bytes(
        exe,
        "KBO salary arbitration withdraw UI hide branch",
        OOTP27_ARBITRATION_WITHDRAW_UI_HIDE_BRANCH_RVA,
        expected_ui_hide_branch,
        patch_ui_hide_branch,
        sizeof(expected_ui_hide_branch));
    ok |= patch_kbo_salary_arbitration_bytes(
        exe,
        "KBO salary arbitration withdraw action gate",
        OOTP27_ARBITRATION_WITHDRAW_ACTION_GATE_RVA,
        expected_action_gate,
        patch_action_gate,
        sizeof(expected_action_gate));

    uint8_t* final_zero_stub = build_kbo_salary_arbitration_final_zero_tender_stub(
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_GATE_RVA + sizeof(expected_final_zero_tender_gate)),
        (void*)((uintptr_t)exe + 0x00681C06u));
    if (final_zero_stub != NULL) {
        ok |= patch_kbo_salary_arbitration_r11_detour(
            exe,
            "KBO salary arbitration AI final zero-offer tender floor",
            OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_GATE_RVA,
            expected_final_zero_tender_gate,
            sizeof(expected_final_zero_tender_gate),
            final_zero_stub);
    } else {
        append_log_line("failed to allocate KBO salary arbitration final zero-offer tender floor stub");
    }

    uint8_t* ai_offer_stub_681012 = build_kbo_salary_arbitration_ai_offer_write_681012_stub(
        (void*)((uintptr_t)exe + 0x00682020u),
        (void*)((uintptr_t)exe + 0x0068204Au));
    if (ai_offer_stub_681012 != NULL) {
        ok |= patch_kbo_salary_arbitration_r11_detour(
            exe,
            "KBO salary arbitration AI offer floor 681012",
            OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_RVA,
            expected_ai_offer_write_681012,
            sizeof(expected_ai_offer_write_681012),
            ai_offer_stub_681012);
    } else {
        append_log_line("failed to allocate KBO salary arbitration AI offer floor 681012 stub");
    }

    uint8_t* zero_offer_check_stub_682089 = build_kbo_salary_arbitration_zero_offer_check_682089_stub(
        (void*)((uintptr_t)exe + 0x006820B7u),
        (void*)((uintptr_t)exe + 0x0068209Bu));
    if (zero_offer_check_stub_682089 != NULL) {
        ok |= patch_kbo_salary_arbitration_r11_detour(
            exe,
            "KBO salary arbitration AI zero-offer final check 682089",
            OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_RVA,
            expected_zero_offer_check_682089,
            sizeof(expected_zero_offer_check_682089),
            zero_offer_check_stub_682089);
    } else {
        append_log_line("failed to allocate KBO salary arbitration AI zero-offer final check 682089 stub");
    }

    uint8_t* high_limit_non_tender_stub_6820af = build_kbo_salary_arbitration_high_limit_non_tender_6820af_stub(
        (void*)((uintptr_t)exe + 0x006821FFu),
        (void*)((uintptr_t)exe + 0x006820C1u));
    if (high_limit_non_tender_stub_6820af != NULL) {
        ok |= patch_kbo_salary_arbitration_r11_detour(
            exe,
            "KBO salary arbitration high-limit non-tender gate 6820af",
            OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_GATE_RVA,
            expected_high_limit_non_tender_gate_6820af,
            sizeof(expected_high_limit_non_tender_gate_6820af),
            high_limit_non_tender_stub_6820af);
    } else {
        append_log_line("failed to allocate KBO salary arbitration high-limit non-tender gate 6820af stub");
    }

    uint8_t* ai_offer_stub_6827cd = build_kbo_salary_arbitration_ai_offer_write_6827cd_stub(
        (void*)((uintptr_t)exe + 0x02AB4BD8u),
        (void*)((uintptr_t)exe + 0x006827DAu));
    if (ai_offer_stub_6827cd != NULL) {
        ok |= patch_kbo_salary_arbitration_r11_detour(
            exe,
            "KBO salary arbitration AI offer floor 6827cd",
            OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RVA,
            expected_ai_offer_write_6827cd,
            sizeof(expected_ai_offer_write_6827cd),
            ai_offer_stub_6827cd);
    } else {
        append_log_line("failed to allocate KBO salary arbitration AI offer floor 6827cd stub");
    }

    ok |= install_kbo_salary_arbitration_non_tender_function_patch(exe);

    append_logf("KBO salary arbitration no-withdraw/zero-offer patch complete installed_any=%d", ok ? 1 : 0);
    return ok;
}
