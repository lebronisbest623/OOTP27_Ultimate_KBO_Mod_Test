static int install_allstar_voting_begin_prepare_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star voting begin prepare patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[16] = {
        0x8D, 0x53, 0xE4,
        0x80, 0xBE, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0xF3, 0x01, 0x00, 0x00
    };
    const uint8_t april_expected[16] = {
        0x8D, 0x53, 0xE4,
        0x80, 0xBE, 0xE8, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0xF3, 0x01, 0x00, 0x00
    };

    uint8_t* target = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    const uint8_t* expected = may_expected;
    if (target == NULL) {
        target = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }
    if (target == NULL) {
        target = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_VOTING_BEGIN_PREP_SITE_RVA);
        expected = april_expected;
    }

    if (!memory_range_readable(target, sizeof(april_expected))) {
        append_logf("KBO all-star voting begin prepare hook target unreadable target=%p", target);
        return 0;
    }

    int ok = 0;
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        append_logf("KBO all-star voting begin prepare hook already installed target=%p", target);
        ok = 1;
    } else if (memcmp(target, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star voting begin prepare hook", target, sizeof(april_expected));
    } else {
        void* return_address = target + sizeof(april_expected);
        void* no_game_address = target + (OOTP27_ALLSTAR_VOTING_BEGIN_NO_GAME_RVA - OOTP27_ALLSTAR_VOTING_BEGIN_PREP_SITE_RVA);
        uint8_t* stub = build_allstar_voting_begin_prepare_stub(return_address, no_game_address, NULL, layout.game_flag_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star voting begin prepare hook stub");
        } else {
            uint8_t patch[16] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star voting begin prepare hook error=%lu", GetLastError());
            } else {
                memcpy(target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(target, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star voting begin prepare hook target=%p stub=%p return=%p no_game=%p helper=%p",
                    target,
                    stub,
                    return_address,
                    no_game_address,
                    &ootp_kbo_prepare_allstar_voting_begin);
                ok = 1;
            }
        }
    }

    return ok;
}

static int install_allstar_events_prepare_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star events prepare patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[19] = {
        0x41, 0x80, 0xBC, 0x24, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x08,
        0x49, 0x8B, 0xCC,
        0xE8, 0xB9, 0x55, 0xFF, 0xFF
    };
    const uint8_t april_expected[19] = {
        0x41, 0x80, 0xBC, 0x24, 0xE8, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x08,
        0x49, 0x8B, 0xCC,
        0xE8, 0x89, 0x55, 0xFF, 0xFF
    };

    uint8_t* target = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    const uint8_t* expected = may_expected;
    if (target == NULL) {
        target = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }
    if (target == NULL) {
        target = (uint8_t*)((uintptr_t)exe + OOTP27_MAKE_ALLSTAR_EVENTS_PREP_SITE_RVA);
        expected = april_expected;
    }

    if (!memory_range_readable(target, sizeof(april_expected))) {
        append_logf("KBO all-star events prepare hook target unreadable target=%p", target);
        return 0;
    }

    int ok = 0;
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        append_logf("KBO all-star events prepare hook already installed target=%p", target);
        ok = 1;
    } else if (memcmp(target, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star events prepare hook", target, sizeof(april_expected));
    } else {
        void* return_address = target + sizeof(april_expected);
        void* allstar_prep_address = resolve_relative_call_target(target + 14);
        if (allstar_prep_address == NULL) {
            append_log_line("KBO all-star events prepare hook skipped: prep call target unresolved");
            return 0;
        }

        uint8_t* stub = build_allstar_events_prepare_stub(return_address, allstar_prep_address, layout.game_flag_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star events prepare hook stub");
        } else {
            uint8_t patch[19] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90, 0x90, 0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star events prepare hook error=%lu", GetLastError());
            } else {
                memcpy(target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(target, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star events prepare hook target=%p stub=%p return=%p prep=%p helper=%p",
                    target,
                    stub,
                    return_address,
                    allstar_prep_address,
                    &ootp_kbo_prepare_allstar_events);
                ok = 1;
            }
        }
    }

    return ok;
}

