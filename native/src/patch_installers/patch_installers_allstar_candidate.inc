static int install_allstar_candidate_team_split_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star candidate team split patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[27] = {
        0x45, 0x39, 0xAE, 0x44, 0x4A, 0x00, 0x00,
        0x7E, 0x0C,
        0x49, 0x8B, 0x86, 0x38, 0x4A, 0x00, 0x00,
        0x4C, 0x8B, 0x30,
        0xEB, 0x03,
        0x4D, 0x8B, 0xF5,
        0x45, 0x8B, 0xFD
    };
    const uint8_t april_expected[27] = {
        0x45, 0x39, 0xAE, 0x3C, 0x4A, 0x00, 0x00,
        0x7E, 0x0C,
        0x49, 0x8B, 0x86, 0x30, 0x4A, 0x00, 0x00,
        0x4C, 0x8B, 0x30,
        0xEB, 0x03,
        0x4D, 0x8B, 0xF5,
        0x45, 0x8B, 0xFD
    };

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_SITE_RVA);
    const uint8_t* expected = april_expected;
    if (memory_range_readable(target, sizeof(may_expected)) && memcmp(target, may_expected, sizeof(may_expected)) == 0) {
        expected = may_expected;
    } else if (!memory_range_readable(target, sizeof(april_expected)) || memcmp(target, april_expected, sizeof(april_expected)) != 0) {
        uint8_t* found = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
        if (found == NULL) {
            found = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
            if (found != NULL) { expected = april_expected; }
        } else {
            expected = may_expected;
        }
        if (found != NULL) {
            uintptr_t dist = found > target ? (uintptr_t)(found - target) : (uintptr_t)(target - found);
            append_logf("KBO all-star candidate team split hook: RVA mismatch, pattern found at %p dist=0x%zx from RVA", found, (size_t)dist);
            target = found;
        }
    }

    if (!memory_range_readable(target, sizeof(april_expected))) {
        append_logf("KBO all-star candidate team split hook target unreadable target=%p", target);
        return 0;
    }

    int ok = 0;
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        append_logf("KBO all-star candidate team split hook already installed target=%p", target);
        ok = 1;
    } else if (memcmp(target, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star candidate team split hook", target, sizeof(april_expected));
        log_extended_context("KBO all-star candidate team split hook", target, 16, 96);
    } else {
        void* return_address = target + sizeof(april_expected);
        void* seeded_address = target + (OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_DONE_RVA - OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_SITE_RVA);
        void* vector_push_back_address = resolve_relative_call_target(
            target + OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_VECTOR_CALL_OFFSET);
        if (vector_push_back_address == NULL) {
            OotpBuildInfo info = read_ootp_build_info();
            uint32_t vector_rva = (info.ok && info.timestamp == KBO_SUPPORTED_OOTP_BUILD_STEAM_2026_05_04_TIMESTAMP)
                ? 0x01CBAA40u
                : OOTP27_VECTOR_PUSH_BACK_RVA;
            vector_push_back_address = (void*)((uintptr_t)exe + vector_rva);
        }

        uint8_t* stub = build_allstar_candidate_team_split_stub(
            return_address,
            seeded_address,
            vector_push_back_address,
            layout.subleague_array_offset,
            layout.subleague_count_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star candidate team split hook stub");
        } else {
            uint8_t patch[27] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                0x90, 0x90, 0x90, 0x90, 0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star candidate team split hook error=%lu", GetLastError());
            } else {
                memcpy(target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(target, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star candidate team split hook target=%p stub=%p return=%p seeded=%p vector_push=%p helper=%p",
                    target,
                    stub,
                    return_address,
                    seeded_address,
                    vector_push_back_address,
                    &ootp_kbo_seed_single_division_allstar_candidate_teams);
                ok = 1;
            }
        }
    }

    return ok;
}
