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

    const uint8_t schedule_context_may[] = {
        0x41, 0x80, 0xBD, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x7F,
        0x41, 0xC6, 0x85, 0xF0, 0x45, 0x00, 0x00, 0x01,
        0x41, 0x0F, 0xB6, 0x85, 0x22, 0x01, 0x00, 0x00
    };
    const uint8_t schedule_expected[2] = {0x74, 0x7F};
    const uint8_t schedule_patch[2] = {0x90, 0x90};
    int schedule_ok = kbo_patch_static_pattern(
        "KBO all-star schedule day import gate",
        schedule_context_may,
        sizeof(schedule_context_may),
        8,
        schedule_expected,
        schedule_patch,
        sizeof(schedule_expected));
    if (!schedule_ok) {
        uint8_t* schedule_day_gate = (uint8_t*)((uintptr_t)exe + OOTP27_IMPORT_SCHEDULE_ALLSTAR_DAY_GATE_RVA);
        if (memory_range_readable(schedule_day_gate, sizeof(schedule_expected))) {
            schedule_ok = patch_static_bytes(
                "KBO all-star schedule day import gate",
                schedule_day_gate,
                schedule_expected,
                schedule_patch,
                sizeof(schedule_expected));
        } else {
            append_log_line("KBO all-star schedule day import gate skipped: fallback RVA unreadable");
        }
    } else {
        ok |= schedule_ok;
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

