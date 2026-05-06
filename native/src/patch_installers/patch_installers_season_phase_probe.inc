static int install_kbo_season_phase_write_probe_patch(
    const char* label,
    uint32_t site_rva,
    const uint8_t* expected,
    size_t expected_size,
    uint8_t base_reg,
    uint8_t value)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_logf("%s skipped reason=no_exe", label);
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + site_rva);
    if (!memory_range_readable(target, expected_size)) {
        append_logf("%s skipped target=%p reason=unreadable", label, target);
        return 0;
    }

    if (target[0] == 0xE8) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }

    if (memcmp(target, expected, expected_size) != 0) {
        log_patch_bytes_mismatch(label, target, expected_size);
        return 0;
    }

    uint8_t* stub = build_kbo_season_phase_write_probe_call_stub(target, base_reg, site_rva, value);
    if (stub == NULL) {
        append_logf("%s skipped target=%p reason=stub_alloc_failed", label, target);
        return 0;
    }

    intptr_t rel = (intptr_t)stub - ((intptr_t)target + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) {
        append_logf("%s skipped target=%p stub=%p reason=stub_too_far", label, target, stub);
        return 0;
    }

    uint8_t patch[8] = {0};
    patch[0] = 0xE8;
    write_u32(&patch[1], (uint32_t)(int32_t)rel);
    for (size_t i = 5; i < expected_size && i < sizeof(patch); i++) {
        patch[i] = 0x90;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, expected_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("%s VirtualProtect failed target=%p error=%lu", label, target, GetLastError());
        return 0;
    }

    memcpy(target, patch, expected_size);
    FlushInstructionCache(GetCurrentProcess(), target, expected_size);

    DWORD ignored = 0;
    VirtualProtect(target, expected_size, old_protect, &ignored);

    append_logf("%s installed target=%p stub=%p rva=0x%x base=%u value=%u", label, target, stub, site_rva, (unsigned)base_reg, (unsigned)value);
    return 1;
}

static int install_kbo_season_phase_write_probe_patches(void)
{
    int ok = 1;
    const uint8_t write_1[] = { 0x41, 0xC6, 0x87, 0xF0, 0x44, 0x00, 0x00, 0x01 };
    const uint8_t write_0[] = { 0x41, 0xC6, 0x87, 0xF0, 0x44, 0x00, 0x00, 0x00 };
    const uint8_t write_4[] = { 0x41, 0xC6, 0x87, 0xF0, 0x44, 0x00, 0x00, 0x04 };
    const uint8_t write_2_r14[] = { 0x41, 0xC6, 0x86, 0xF0, 0x44, 0x00, 0x00, 0x02 };
    const uint8_t write_2_rbx[] = { 0xC6, 0x83, 0xF0, 0x44, 0x00, 0x00, 0x02 };
    const uint8_t write_2_rdx[] = { 0xC6, 0x82, 0xF0, 0x44, 0x00, 0x00, 0x02 };
    const uint8_t write_3_rdi[] = { 0xC6, 0x87, 0xF0, 0x44, 0x00, 0x00, 0x03 };
    const uint8_t write_3_rcx[] = { 0xC6, 0x81, 0xF0, 0x44, 0x00, 0x00, 0x03 };
    const uint8_t write_3_rsi[] = { 0xC6, 0x86, 0xF0, 0x44, 0x00, 0x00, 0x03 };

    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 1", OOTP27_SEASON_PHASE_WRITE_1_RVA, write_1, sizeof(write_1), 15, 1);
    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 0", OOTP27_SEASON_PHASE_WRITE_0_RVA, write_0, sizeof(write_0), 15, 0);
    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 4", OOTP27_SEASON_PHASE_WRITE_4_RVA, write_4, sizeof(write_4), 15, 4);
    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 2a", OOTP27_SEASON_PHASE_WRITE_2A_RVA, write_2_r14, sizeof(write_2_r14), 14, 2);
    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 2b", OOTP27_SEASON_PHASE_WRITE_2B_RVA, write_2_rbx, sizeof(write_2_rbx), 3, 2);
    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 2c", OOTP27_SEASON_PHASE_WRITE_2C_RVA, write_2_rdx, sizeof(write_2_rdx), 2, 2);
    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 3a", OOTP27_SEASON_PHASE_WRITE_3A_RVA, write_3_rdi, sizeof(write_3_rdi), 7, 3);
    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 3b", OOTP27_SEASON_PHASE_WRITE_3B_RVA, write_3_rcx, sizeof(write_3_rcx), 1, 3);
    ok &= install_kbo_season_phase_write_probe_patch("KBO season phase write probe 3c", OOTP27_SEASON_PHASE_WRITE_3C_RVA, write_3_rsi, sizeof(write_3_rsi), 6, 3);
    return ok;
}
