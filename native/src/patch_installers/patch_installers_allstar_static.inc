static int install_single_division_allstar_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO single-division all-star patch");
    if (exe == NULL) {
        return 0;
    }

    int ok = 0;

    const uint8_t prep_context_may[] = {
        0xF6, 0x42, 0x4C, 0x01,
        0x0F, 0x84, 0xC0, 0x00, 0x00, 0x00
    };
    const uint8_t prep_expected[6] = {
        0x0F, 0x84, 0xC0, 0x00, 0x00, 0x00
    };
    const uint8_t prep_patch[6] = {
        0xE9, 0xC1, 0x00, 0x00, 0x00, 0x90
    };
    ok |= kbo_patch_static_pattern(
        "KBO single-division all-star prep gate",
        prep_context_may,
        sizeof(prep_context_may),
        4,
        prep_expected,
        prep_patch,
        sizeof(prep_expected));
    if (!ok) {
        uint8_t* prep_gate = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_PREP_SINGLE_DIVISION_GATE_RVA);
        if (memory_range_readable(prep_gate, sizeof(prep_expected))) {
            ok |= patch_static_bytes(
                "KBO single-division all-star prep gate",
                prep_gate,
                prep_expected,
                prep_patch,
                sizeof(prep_expected));
        }
    }

    const uint8_t roster_context_may[] = {
        0x41, 0x8B, 0x46, 0x4C,
        0xA8, 0x01,
        0x74, 0x0D
    };
    const uint8_t roster_expected[2] = {0x74, 0x0D};
    const uint8_t roster_patch[2] = {0xEB, 0x0D};
    int roster_ok = kbo_patch_static_pattern(
        "KBO single-division all-star roster gate",
        roster_context_may,
        sizeof(roster_context_may),
        6,
        roster_expected,
        roster_patch,
        sizeof(roster_expected));
    if (!roster_ok) {
        uint8_t* roster_gate = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_ROSTER_SINGLE_DIVISION_GATE_RVA);
        if (memory_range_readable(roster_gate, sizeof(roster_expected))) {
            roster_ok = patch_static_bytes(
                "KBO single-division all-star roster gate",
                roster_gate,
                roster_expected,
                roster_patch,
                sizeof(roster_expected));
        }
    }
    ok |= roster_ok;

    const uint8_t schedule_expected_may[] = {
        0x41, 0x80, 0xBD, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x7F,
        0x41, 0xC6, 0x85, 0xF0, 0x45, 0x00, 0x00, 0x01
    };
    const uint8_t schedule_expected_april[] = {
        0x41, 0x80, 0xBD, 0xE8, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x7F,
        0x41, 0xC6, 0x85, 0xE8, 0x45, 0x00, 0x00, 0x01
    };
    uint8_t* schedule_target = find_ootp_executable_pattern(schedule_expected_may, sizeof(schedule_expected_may));
    const uint8_t* schedule_expected = schedule_expected_may;
    if (schedule_target == NULL) {
        schedule_target = find_ootp_executable_pattern(schedule_expected_april, sizeof(schedule_expected_april));
        schedule_expected = schedule_expected_april;
    }
    if (schedule_target == NULL) {
        schedule_target = (uint8_t*)((uintptr_t)exe + OOTP27_IMPORT_SCHEDULE_ALLSTAR_DAY_GATE_RVA - 8u);
        schedule_expected = schedule_expected_april;
    }

    int schedule_ok = 0;
    if (!memory_range_readable(schedule_target, sizeof(schedule_expected_may))) {
        append_logf("KBO all-star schedule import capture hook target unreadable target=%p", schedule_target);
    } else if (is_rip_absolute_jump_patch(schedule_target) || is_rax_absolute_jump_patch(schedule_target)) {
        append_logf("KBO all-star schedule import capture hook already installed target=%p", schedule_target);
        schedule_ok = 1;
    } else if (memcmp(schedule_target, schedule_expected, sizeof(schedule_expected_may)) != 0) {
        log_patch_bytes_mismatch("KBO all-star schedule import capture hook", schedule_target, sizeof(schedule_expected_may));
    } else {
        void* return_address = schedule_target + sizeof(schedule_expected_may);
        KboAllstarLayout layout = kbo_get_allstar_layout();
        uint8_t* stub = build_allstar_schedule_import_capture_stub(return_address, layout.game_flag_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star schedule import capture hook stub");
        } else {
            uint8_t patch[18] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90, 0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(schedule_target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star schedule import capture hook error=%lu", GetLastError());
            } else {
                memcpy(schedule_target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), schedule_target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(schedule_target, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star schedule import capture hook target=%p stub=%p return=%p helper=%p",
                    schedule_target,
                    stub,
                    return_address,
                    &ootp_kbo_capture_allstar_schedule_import_league);
                schedule_ok = 1;
            }
        }
    }
    ok |= schedule_ok;

    return ok;
}

static int install_allstar_team_setup_single_division_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star team setup single-division patch");
    if (exe == NULL) {
        return 0;
    }

    const uint8_t context_may[] = {
        0xF6, 0x41, 0x4C, 0x01,
        0x0F, 0x85, 0x74, 0x02, 0x00, 0x00
    };
    const uint8_t expected[6] = {
        0x0F, 0x85, 0x74, 0x02, 0x00, 0x00
    };
    const uint8_t patch[6] = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    int ok = kbo_patch_static_pattern(
        "KBO all-star team setup single-division bail gate",
        context_may,
        sizeof(context_may),
        4,
        expected,
        patch,
        sizeof(expected));
    if (ok) {
        return 1;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + OOTP27_ALLSTAR_TEAM_SETUP_SINGLE_DIVISION_BAIL_RVA);
    if (!memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO all-star team setup single-division bail gate target unreadable target=%p", target);
        return 0;
    }

    return patch_static_bytes(
        "KBO all-star team setup single-division bail gate",
        target,
        expected,
        patch,
        sizeof(expected));
}
