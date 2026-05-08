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
