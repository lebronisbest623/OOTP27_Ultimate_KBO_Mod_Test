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

    const uint8_t expected_high_limit_skip_rsi_branch[6] = {
        0x0F, 0x8F, 0xDE, 0x02, 0x00, 0x00              /* jg 0x1406820b3 */
    };
    const uint8_t patch_high_limit_skip_rsi_branch[6] = {
        0x0F, 0x8F, 0xDA, 0x02, 0x00, 0x00              /* jg 0x1406820af */
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
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_CONTINUE_RVA));
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
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_FLOOR_RETURN_RVA),
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_PASS_RETURN_RVA));
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
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_TENDER_RETURN_RVA),
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_PASS_RETURN_RVA));
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

    ok |= patch_kbo_salary_arbitration_bytes(
        exe,
        "KBO salary arbitration high-limit skip-rsi branch retarget",
        OOTP27_ARBITRATION_HIGH_LIMIT_SKIP_RSI_BRANCH_RVA,
        expected_high_limit_skip_rsi_branch,
        patch_high_limit_skip_rsi_branch,
        sizeof(expected_high_limit_skip_rsi_branch));

    uint8_t* high_limit_non_tender_stub_6820af = build_kbo_salary_arbitration_high_limit_non_tender_6820af_stub(
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_6820AF_PASS_RETURN_RVA),
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_6820AF_FLOOR_RETURN_RVA));
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
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_SUPERSTAR_SOURCE_RVA),
        (void*)((uintptr_t)exe + OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RETURN_RVA));
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
