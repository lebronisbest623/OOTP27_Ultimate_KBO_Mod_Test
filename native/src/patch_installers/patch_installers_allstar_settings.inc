static int install_allstar_settings_ui_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star settings UI patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[16] = {
        0x41, 0x0F, 0xB6, 0x95, 0xF0, 0x45, 0x00, 0x00,
        0x48, 0x8B, 0xCF,
        0xE8, 0x7C, 0xBC, 0x92, 0x00
    };
    const uint8_t april_expected[16] = {
        0x41, 0x0F, 0xB6, 0x95, 0xE8, 0x45, 0x00, 0x00,
        0x48, 0x8B, 0xCF,
        0xE8, 0xB9, 0x60, 0x93, 0x00
    };

    uint8_t* value_site = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    const uint8_t* expected = may_expected;
    if (value_site == NULL) {
        value_site = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }
    int ok_value = 0;
    if (value_site == NULL || !memory_range_readable(value_site, sizeof(april_expected))) {
        append_logf("KBO all-star settings value hook target unreadable target=%p", value_site);
    } else if (is_rip_absolute_jump_patch(value_site) || is_rax_absolute_jump_patch(value_site)) {
        append_logf("KBO all-star settings value hook already installed target=%p", value_site);
        ok_value = 1;
    } else if (memcmp(value_site, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star settings value hook", value_site, sizeof(april_expected));
    } else {
        void* return_address = value_site + sizeof(april_expected);
        void* checkbox_set_bool_address = resolve_relative_call_target(value_site + 11);
        if (checkbox_set_bool_address == NULL) {
            OotpBuildInfo info = read_ootp_build_info();
            uint32_t checkbox_rva = kbo_ootp_build_is_steam_2026_05_04(info)
                ? 0x01E717D0u
                : OOTP27_UI_CHECKBOX_SET_BOOL_RVA;
            checkbox_set_bool_address = (void*)((uintptr_t)exe + checkbox_rva);
        }

        uint8_t* stub = build_allstar_settings_enable_stub(return_address, checkbox_set_bool_address, layout.game_flag_offset);
        if (stub == NULL) {
            append_log_line("failed to allocate KBO all-star settings value hook stub");
        } else {
            uint8_t patch[16] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(value_site, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                append_logf("VirtualProtect failed for KBO all-star settings value hook error=%lu", GetLastError());
            } else {
                memcpy(value_site, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), value_site, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(value_site, sizeof(patch), old_protect, &ignored);
                append_logf(
                    "installed KBO all-star settings value hook target=%p stub=%p return=%p checkbox_set_bool=%p helper=%p",
                    value_site,
                    stub,
                    return_address,
                    checkbox_set_bool_address,
                    &ootp_kbo_enable_allstar_setting);
                ok_value = 1;
            }
        }
    }

    const uint8_t disabled_message_context_may[] = {
        0x45, 0x84, 0xFF,
        0x41, 0xBF, 0x01, 0x00, 0x00, 0x00,
        0x74, 0x44,
        0x45, 0x33, 0xC0,
        0x48, 0x8D, 0x15
    };
    const uint8_t disabled_message_expected_may[2] = {
        0x74, 0x44
    };
    const uint8_t disabled_message_patch_may[2] = {
        0xEB, 0x44
    };
    int ok_disable = kbo_patch_static_pattern(
        "KBO all-star settings disabled-message gate",
        disabled_message_context_may,
        sizeof(disabled_message_context_may),
        9,
        disabled_message_expected_may,
        disabled_message_patch_may,
        sizeof(disabled_message_expected_may));

    const uint8_t game_flag_skip_context_may[] = {
        0x41, 0x80, 0xBD, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0x2A, 0x15, 0x00, 0x00
    };
    const uint8_t game_flag_skip_expected_may[6] = {
        0x0F, 0x84, 0x2A, 0x15, 0x00, 0x00
    };
    const uint8_t game_flag_skip_patch_may[6] = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    int ok_game_flag_skip = kbo_patch_static_pattern(
        "KBO all-star settings game-flag skip gate",
        game_flag_skip_context_may,
        sizeof(game_flag_skip_context_may),
        8,
        game_flag_skip_expected_may,
        game_flag_skip_patch_may,
        sizeof(game_flag_skip_expected_may));

    const uint8_t one_division_warning_context_may[] = {
        0x41, 0x80, 0xBD, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0x2A, 0x15, 0x00, 0x00,
        0x66, 0x83, 0xBB, 0xFA, 0x03, 0x00, 0x00, 0x01,
        0x0F, 0x8C
    };
    const uint8_t one_division_warning_expected_may[6] = {
        0x0F, 0x8C, 0x87, 0x0B, 0x00, 0x00
    };
    const uint8_t one_division_warning_patch_may[6] = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    int ok_warning = kbo_patch_static_pattern(
        "KBO all-star settings one-division warning gate",
        one_division_warning_context_may,
        sizeof(one_division_warning_context_may),
        22,
        one_division_warning_expected_may,
        one_division_warning_patch_may,
        sizeof(one_division_warning_expected_may));
    return ok_value || ok_disable || ok_game_flag_skip || ok_warning;
}
