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

