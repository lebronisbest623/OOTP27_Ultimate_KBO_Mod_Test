#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_log.h"
#include "../../foreign/signability/submit_offer_probe_state.h"
#include "no_minor_contract_callback_patch.h"
#include "no_minor_demand_write_floor_patches.h"
#include "no_minor_experimental_patch.h"
#include "no_minor_foreign_fa_baseline_patch.h"
#include "no_minor_offer_callback_patch.h"
#include "no_minor_offer_demand_floor_patch.h"
#include "no_minor_patch_helpers.h"
#include "no_minor_player_action_patch.h"
#include "no_minor_scan_patch.h"
#include "no_minor_string_patch.h"
#include "no_minor_submit_salary_floor_patch.h"
int install_kbo_no_minor_contract_experimental_patch(void)
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

    kbo_enable_no_minor_contract_demand_floor();

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
    const uint8_t context_dynamic_zero_branch[48] = {
        0xEB, 0x60, 0x41, 0x88, 0xBF, 0x14, 0x09, 0x00,
        0x00, 0x45, 0x32, 0xE4, 0x85, 0xFF, 0x75, 0x04,
        0x32, 0xC0, 0xEB, 0x43, 0x66, 0x41, 0x83, 0xBF,
        0xD4, 0x08, 0x00, 0x00, 0x00, 0x75, 0x36, 0x48,
        0x8B, 0x85, 0x10, 0x12, 0x00, 0x00, 0x0F, 0xB7,
        0x80, 0xF4, 0x44, 0x00, 0x00, 0x66, 0x41, 0x89
    };
    const uint8_t context_dynamic_zero_branch_mask[48] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,0,1,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1
    };
    const uint8_t expected_dynamic_reset[7] = {0x41, 0x88, 0xB7, 0xA8, 0x08, 0x00, 0x00};
    const uint8_t patch_dynamic_reset[7] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    const uint8_t context_dynamic_reset[48] = {
        0x41, 0xB4, 0x01, 0x0F, 0xB6, 0xC0, 0x41, 0x39,
        0xB4, 0x87, 0xD8, 0x08, 0x00, 0x00, 0x75, 0x31,
        0x41, 0x88, 0xB7, 0xA8, 0x08, 0x00, 0x00, 0x66,
        0x41, 0xC7, 0x87, 0x14, 0x09, 0x00, 0x00, 0x01,
        0x00, 0x41, 0x8B, 0x87, 0xB4, 0x00, 0x00, 0x00,
        0x41, 0x89, 0x87, 0xCC, 0x08, 0x00, 0x00, 0x45
    };
    const uint8_t context_dynamic_reset_mask[48] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1
    };
    const uint8_t expected_offer_major_flag_copy[8] = {0x88, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x84, 0xC0};
    const uint8_t patch_offer_major_flag_copy[8] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x01, 0x90};
    const uint8_t expected_offer_major_flag_reset[7] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x00};
    const uint8_t patch_offer_major_flag_reset[7] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x01};
    const uint8_t context_offer_major_flag_reset[64] = {
        0x00, 0x00, 0x8B, 0x45, 0xC0, 0x89, 0x87, 0xC8,
        0x00, 0x00, 0x00, 0xE9, 0xAF, 0x06, 0x00, 0x00,
        0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x00, 0xE9,
        0xA3, 0x06, 0x00, 0x00, 0x49, 0x83, 0xF8, 0x08,
        0x75, 0x61, 0x48, 0x8B, 0x05, 0x39, 0xA0, 0xE3,
        0x01, 0x48, 0x8B, 0x88, 0xD8, 0x02, 0x00, 0x00,
        0x48, 0x8B, 0x80, 0xE0, 0x02, 0x00, 0x00, 0x8B,
        0x50, 0x48, 0xE8, 0xC3, 0x37, 0x7D, 0xFF, 0x48
    };
    const uint8_t context_offer_major_flag_reset_mask[64] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 0,0,0,0,1,1,1,1,
        1,0,1,1,1,0,0,0, 0,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,1,0,0,0,0,1
    };
    const uint8_t expected_option_text_branch[2] = {0x75, 0x07};
    const uint8_t patch_option_text_branch[2] = {0x90, 0x90};
    const uint8_t context_option_text_branch[64] = {
        0x8B, 0xD3, 0x45, 0x38, 0x8D, 0xAA, 0x08, 0x00,
        0x00, 0x4C, 0x8D, 0x05, 0x35, 0x99, 0x7D, 0x01,
        0x75, 0x07, 0x4C, 0x8D, 0x05, 0xC4, 0xC0, 0x7A,
        0x01, 0xE8, 0x47, 0x2B, 0xDE, 0xFF, 0x48, 0x8B,
        0xD0, 0x48, 0x8D, 0x4D, 0x97, 0xE8, 0xDB, 0xFF,
        0x8C, 0x00, 0xB9, 0x00, 0x0D, 0x00, 0x00, 0xE8,
        0xAD, 0x69, 0x39, 0x01, 0x48, 0x89, 0x45, 0x77,
        0x45, 0x33, 0xC9, 0x45, 0x33, 0xC0, 0x33, 0xD2
    };
    const uint8_t context_option_text_branch_mask[64] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,0,0,0,0,
        1,0,1,1,1,0,0,0, 0,1,0,0,0,0,1,1,
        1,1,1,1,1,1,0,0, 0,0,1,1,1,1,1,1,
        0,0,0,0,1,1,1,1, 0,1,1,1,1,1,1,1
    };
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
    const uint8_t context_minor_extension_action_case[64] = {
        0x4E, 0x08, 0xE8, 0xE7, 0xFD, 0x7D, 0xFF, 0x41,
        0x0F, 0xB6, 0xC6, 0xE9, 0x95, 0xFB, 0xFF, 0xFF,
        0x83, 0xFF, 0x36, 0x0F, 0x85, 0x43, 0x01, 0x00,
        0x00, 0x4C, 0x8B, 0x1D, 0xDE, 0x74, 0x17, 0x02,
        0x49, 0x8B, 0x83, 0xE0, 0x02, 0x00, 0x00, 0x8B,
        0x50, 0x58, 0x49, 0x8B, 0xCB, 0xE8, 0xBC, 0x2C,
        0x3C, 0xFF, 0x48, 0x8B, 0xF8, 0x48, 0x85, 0xC0,
        0x0F, 0x84, 0x65, 0xFB, 0xFF, 0xFF, 0x41, 0x83
    };
    const uint8_t context_minor_extension_action_case_mask[64] = {
        1,1,1,0,0,0,0,1, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,0,0,0, 0,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,1, 1,1,0,0,0,0,1,1
    };
    const uint8_t context_offer_ui_minor_item_17a93df[64] = {
        0xF8, 0x02, 0x74, 0x0C, 0x80, 0x7E, 0x08, 0x00,
        0x75, 0x2A, 0x80, 0x7E, 0x0A, 0x00, 0x75, 0x24,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0xEF, 0xAC,
        0x3E, 0x01, 0x41, 0x8D, 0x51, 0x01, 0xE8, 0x6E,
        0x17, 0xA2, 0xFF, 0x48, 0x8B, 0xD0, 0x45, 0x33,
        0xC9, 0x45, 0x33, 0xC0, 0x49, 0x8B, 0xCC, 0xE8,
        0x5D, 0xAB, 0x3F, 0xFF, 0x41, 0x80, 0xBE, 0xD1,
        0x00, 0x00, 0x00, 0x00, 0x0F, 0x85, 0x80, 0x00
    };
    const uint8_t context_offer_ui_minor_item_17a93df_mask[64] = {
        1,1,1,0,1,1,0,1,
        1,0,1,1,0,1,1,0,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        0,0,0,0,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_offer_ui_option_item_17a9467[64] = {
        0x8B, 0x50, 0x58, 0x49, 0x8B, 0xCD, 0xE8, 0xAE,
        0x3D, 0x0A, 0xFF, 0x83, 0xF8, 0x02, 0x75, 0x2A,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0x8F, 0x31,
        0x45, 0x01, 0xBB, 0x01, 0x00, 0x00, 0x00, 0x8B,
        0xD3, 0xE8, 0xE3, 0x16, 0xA2, 0xFF, 0x48, 0x8B,
        0xD0, 0x45, 0x33, 0xC9, 0x44, 0x8D, 0x43, 0x01,
        0x49, 0x8B, 0xCC, 0xE8, 0xD1, 0xAA, 0x3F, 0xFF,
        0xEB, 0x05, 0xBB, 0x01, 0x00, 0x00, 0x00, 0x41
    };
    const uint8_t context_offer_ui_option_item_17a9467_mask[64] = {
        1,1,1,1,1,1,1,0,
        0,0,0,1,1,1,1,0,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,1,
        1,1,0,0,0,0,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_offer_ui_selection_value_17a94a0[64] = {
        0x05, 0xBB, 0x01, 0x00, 0x00, 0x00, 0x41, 0xB0,
        0x01, 0x49, 0x8B, 0xCC, 0x80, 0x7E, 0x0A, 0x00,
        0xBA, 0x02, 0x00, 0x00, 0x00, 0x75, 0x04, 0x0F,
        0xB6, 0x56, 0x08, 0xE8, 0x10, 0x6B, 0x6C, 0x00,
        0x44, 0x8B, 0xC3, 0x8B, 0xD3, 0x4D, 0x8B, 0xCC,
        0x48, 0x8B, 0xCF, 0xE8, 0x90, 0xCC, 0x6A, 0x00,
        0x41, 0xBC, 0x02, 0x00, 0x00, 0x00, 0xEB, 0x03,
        0x49, 0x8B, 0xDC, 0x0F, 0xB6, 0x46, 0x08, 0x84
    };
    const uint8_t context_offer_ui_selection_value_17a94a0_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,0,1,
        1,1,1,1,1,1,0,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_offer_ui_selection_branch_17a94a5[64] = {
        0x00, 0x41, 0xB0, 0x01, 0x49, 0x8B, 0xCC, 0x80,
        0x7E, 0x0A, 0x00, 0xBA, 0x02, 0x00, 0x00, 0x00,
        0x75, 0x04, 0x0F, 0xB6, 0x56, 0x08, 0xE8, 0x10,
        0x6B, 0x6C, 0x00, 0x44, 0x8B, 0xC3, 0x8B, 0xD3,
        0x4D, 0x8B, 0xCC, 0x48, 0x8B, 0xCF, 0xE8, 0x90,
        0xCC, 0x6A, 0x00, 0x41, 0xBC, 0x02, 0x00, 0x00,
        0x00, 0xEB, 0x03, 0x49, 0x8B, 0xDC, 0x0F, 0xB6,
        0x46, 0x08, 0x84, 0xC0, 0x75, 0x05, 0x38, 0x46
    };
    const uint8_t context_offer_ui_selection_branch_17a94a5_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,0,1,1,1,1,1,1,
        1,0,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,0,1,1
    };
    const uint8_t context_offer_report_minor_item_17da0ec[64] = {
        0x48, 0x89, 0x7D, 0x00, 0x80, 0x7E, 0x08, 0x00,
        0x75, 0x78, 0x80, 0x7E, 0x0A, 0x00, 0x75, 0x72,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0xE2, 0x9F,
        0x3B, 0x01, 0xE8, 0x65, 0x0A, 0x9F, 0xFF, 0x48,
        0x8B, 0xD0, 0x48, 0x8D, 0x4C, 0x24, 0x48, 0xE8,
        0xF8, 0xDE, 0x4D, 0x00, 0xB9, 0x00, 0x0D, 0x00,
        0x00, 0xE8, 0xCA, 0x48, 0xFA, 0x00, 0x48, 0x89,
        0x85, 0x88, 0x00, 0x00, 0x00, 0x45, 0x33, 0xC9
    };
    const uint8_t context_offer_report_minor_item_17da0ec_mask[64] = {
        1,1,1,0,1,1,0,1,
        1,0,1,1,0,1,1,0,
        1,1,1,1,1,1,0,0,
        0,0,1,0,0,0,0,1,
        1,1,1,1,1,1,1,1,
        0,0,0,0,1,1,1,1,
        1,1,0,0,0,0,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_offer_report_option_item_17da2c6[64] = {
        0x04, 0x0A, 0x66, 0x41, 0xFF, 0xC0, 0x66, 0x45,
        0x3B, 0x86, 0x08, 0x01, 0x00, 0x00, 0x7C, 0xDA,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0x70, 0x76,
        0x3E, 0x01, 0x41, 0x8D, 0x51, 0x01, 0xE8, 0x87,
        0x08, 0x9F, 0xFF, 0x48, 0x8B, 0xD8, 0x48, 0x85,
        0xC0, 0x74, 0x75, 0x48, 0x8B, 0xCF, 0x48, 0xFF,
        0xC1, 0x40, 0x38, 0x34, 0x08, 0x75, 0xF7, 0x83,
        0xC1, 0x02, 0x8B, 0x7C, 0x24, 0x5C, 0x3B, 0xCF
    };
    const uint8_t context_offer_report_option_item_17da2c6_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,0,1,1,1,1,1,
        1,1,1,1,1,1,0,1,
        1,1,1,1,0,1,1,1
    };
    const uint8_t context_contract_type_minor_item_182666a[64] = {
        0x8B, 0xD0, 0x45, 0x33, 0xC9, 0x44, 0x8B, 0xC6,
        0x48, 0x8B, 0xCF, 0xE8, 0xF6, 0xD8, 0x37, 0xFF,
        0x45, 0x33, 0xC0, 0x48, 0x8D, 0x15, 0x64, 0xDA,
        0x36, 0x01, 0x8B, 0xCE, 0xE8, 0xE5, 0x3F, 0x9A,
        0xFF, 0x48, 0x8B, 0xD0, 0x45, 0x33, 0xC9, 0x45,
        0x33, 0xC0, 0x48, 0x8B, 0xCF, 0xE8, 0xD4, 0xD8,
        0x37, 0xFF, 0x41, 0x8B, 0xD4, 0x45, 0x38, 0xA7,
        0xA8, 0x08, 0x00, 0x00, 0x0F, 0x95, 0xC2, 0x41
    };
    const uint8_t context_contract_type_minor_item_182666a_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,0,0,0,
        0,1,1,1,1,1,1,1,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_write_638d32[64] = {
        0x48, 0x8D, 0x55, 0x7F, 0x48, 0x8D, 0x8F, 0x48,
        0x1C, 0x00, 0x00, 0xE8, 0xDE, 0x78, 0x4D, 0x00,
        0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x00, 0x41,
        0x8B, 0x87, 0xC8, 0x4C, 0x00, 0x00, 0x89, 0x45,
        0x9F, 0x85, 0xC0, 0x75, 0x16, 0x8B, 0x87, 0x20,
        0x01, 0x00, 0x00, 0x89, 0x45, 0x9F, 0x85, 0xC0,
        0x75, 0x09, 0x8B, 0x87, 0xDC, 0x01, 0x00, 0x00,
        0x89, 0x45, 0x9F, 0x48, 0x8D, 0x55, 0x9F, 0x48
    };
    const uint8_t context_write_638d32_mask[64] = {
        1,1,1,1,0,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,1,1,1,
        1,1,1,1,1,1,1,1,
        1,0,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_write_6de7c1[64] = {
        0x38, 0x01, 0x00, 0x00, 0x48, 0x8D, 0x8F, 0x48,
        0x1C, 0x00, 0x00, 0xE8, 0x4F, 0x1E, 0x43, 0x00,
        0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x00, 0x48,
        0x85, 0xF6, 0x74, 0x59, 0x48, 0x8B, 0x85, 0x20,
        0x01, 0x00, 0x00, 0x8B, 0x90, 0xC8, 0x4C, 0x00,
        0x00, 0x48, 0x8B, 0xCF, 0xE8, 0x7E, 0xDB, 0x34,
        0x00, 0x48, 0x8B, 0xD8, 0x48, 0x8B, 0x85, 0x20,
        0x01, 0x00, 0x00, 0x8B, 0x90, 0xC8, 0x4C, 0x00
    };
    const uint8_t context_write_6de7c1_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,0,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,0,0,0,
        0,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_write_6ddea2[64] = {
        0x48, 0x8D, 0x55, 0xA8, 0x49, 0x8D, 0x8F, 0x48,
        0x1C, 0x00, 0x00, 0xE8, 0x6E, 0x27, 0x43, 0x00,
        0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x02, 0x66,
        0x83, 0xBF, 0xDC, 0x03, 0x00, 0x00, 0x00, 0x0F,
        0x86, 0xCB, 0xFE, 0xFF, 0xFF, 0x4C, 0x8D, 0x0D,
        0x3A, 0x26, 0x3F, 0x02, 0x4C, 0x8D, 0x05, 0x63,
        0x26, 0x3F, 0x02, 0xBA, 0x01, 0x00, 0x00, 0x00,
        0xE8, 0x91, 0xCC, 0xAE, 0x00, 0x48, 0x8B, 0xD0
    };
    const uint8_t context_write_6ddea2_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,0,0,0,0,1,1,1,
        0,0,0,0,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,0,0,0,0,1,1,1
    };
    const uint8_t context_write_6de33e[64] = {
        0x48, 0x8D, 0x55, 0xB8, 0x48, 0x8D, 0x8B, 0x48,
        0x1C, 0x00, 0x00, 0xE8, 0xD2, 0x22, 0x43, 0x00,
        0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x02, 0x48,
        0x8B, 0xBD, 0x20, 0x01, 0x00, 0x00, 0x66, 0x83,
        0xBF, 0xDC, 0x03, 0x00, 0x00, 0x00, 0x76, 0x27,
        0x4C, 0x8D, 0x0D, 0x9B, 0x21, 0x3F, 0x02, 0x4C,
        0x8D, 0x05, 0xC4, 0x21, 0x3F, 0x02, 0xBA, 0x01,
        0x00, 0x00, 0x00, 0xE8, 0xF2, 0xC7, 0xAE, 0x00
    };
    const uint8_t context_write_6de33e_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,0,
        1,1,1,0,0,0,0,1,
        1,1,0,0,0,0,1,1,
        1,1,1,1,0,0,0,0
    };
    const uint8_t context_write_6de58c[64] = {
        0x48, 0x8D, 0x55, 0xC0, 0x48, 0x8D, 0x8B, 0x48,
        0x1C, 0x00, 0x00, 0xE8, 0x84, 0x20, 0x43, 0x00,
        0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x02, 0x48,
        0x8B, 0xB5, 0x20, 0x01, 0x00, 0x00, 0x66, 0x83,
        0xBE, 0xDC, 0x03, 0x00, 0x00, 0x00, 0x76, 0x27,
        0x4C, 0x8D, 0x0D, 0x4D, 0x1F, 0x3F, 0x02, 0x4C,
        0x8D, 0x05, 0x76, 0x1F, 0x3F, 0x02, 0xBA, 0x01,
        0x00, 0x00, 0x00, 0xE8, 0xA4, 0xC5, 0xAE, 0x00
    };
    const uint8_t context_write_6de58c_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,0,
        1,1,1,0,0,0,0,1,
        1,1,0,0,0,0,1,1,
        1,1,1,1,0,0,0,0
    };
    const uint8_t context_write_6de6bf[64] = {
        0x48, 0x8D, 0x55, 0xC8, 0x48, 0x8D, 0x8B, 0x48,
        0x1C, 0x00, 0x00, 0xE8, 0x51, 0x1F, 0x43, 0x00,
        0xC6, 0x80, 0xA8, 0x08, 0x00, 0x00, 0x02, 0x66,
        0x45, 0x39, 0xA5, 0xDC, 0x03, 0x00, 0x00, 0x76,
        0x27, 0x4C, 0x8D, 0x0D, 0x21, 0x1E, 0x3F, 0x02,
        0x4C, 0x8D, 0x05, 0x4A, 0x1E, 0x3F, 0x02, 0xBA,
        0x01, 0x00, 0x00, 0x00, 0xE8, 0x78, 0xC4, 0xAE,
        0x00, 0x48, 0x8B, 0xD0, 0x48, 0x8D, 0x8B, 0x68
    };
    const uint8_t context_write_6de6bf_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        0,1,1,1,0,0,0,0,
        1,1,1,0,0,0,0,1,
        1,1,1,1,1,0,0,0,
        0,1,1,1,1,1,1,1
    };
    const uint8_t context_write_a4be04[64] = {
        0xFA, 0x44, 0x00, 0x00, 0x8D, 0x41, 0xFE, 0x3C,
        0x04, 0x76, 0x05, 0x80, 0xF9, 0x09, 0x72, 0x23,
        0xC6, 0x87, 0xA8, 0x08, 0x00, 0x00, 0x00, 0xC6,
        0x87, 0x14, 0x09, 0x00, 0x00, 0x01, 0xC6, 0x87,
        0xAA, 0x08, 0x00, 0x00, 0x00, 0x44, 0x89, 0xA7,
        0xC4, 0x08, 0x00, 0x00, 0x44, 0x89, 0xA7, 0xD8,
        0x08, 0x00, 0x00, 0x80, 0xBF, 0xA8, 0x08, 0x00,
        0x00, 0x00, 0x0F, 0x84, 0x15, 0x01, 0x00, 0x00
    };
    const uint8_t context_write_a4be04_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,0,1,1,1,1,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0
    };
    const uint8_t context_write_2184b61[64] = {
        0xFF, 0xFF, 0x90, 0x48, 0x8D, 0x8F, 0xF8, 0x00,
        0x00, 0x00, 0xE8, 0xF0, 0xC9, 0xFD, 0xFF, 0x90,
        0xC6, 0x87, 0xA8, 0x08, 0x00, 0x00, 0x00, 0x48,
        0xC7, 0x87, 0xB0, 0x08, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x48, 0x8D, 0x8F, 0xF8, 0x00, 0x00,
        0x00, 0xE8, 0xB1, 0x28, 0xFE, 0xFF, 0x90, 0x48,
        0x8B, 0xC7, 0x48, 0x8B, 0x5C, 0x24, 0x38, 0x48,
        0x83, 0xC4, 0x20, 0x5F, 0xC3, 0xCC, 0xCC, 0x48
    };
    const uint8_t context_write_2184b61_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,0,0,0,0,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,0,0,0,0,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_write_2185963[64] = {
        0xE8, 0x78, 0xB2, 0xE1, 0xFF, 0x48, 0xC7, 0x87,
        0xB0, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xC6, 0x87, 0xA8, 0x08, 0x00, 0x00, 0x00, 0x4C,
        0x8D, 0x67, 0x18, 0xC6, 0x44, 0x24, 0x20, 0x00,
        0x4C, 0x8B, 0xCB, 0x45, 0x33, 0xC0, 0xBA, 0x2C,
        0x01, 0x00, 0x00, 0x49, 0x8B, 0xCC, 0xE8, 0x5A,
        0xEB, 0xFF, 0xFF, 0x48, 0x8D, 0xAF, 0xF8, 0x00,
        0x00, 0x00, 0x4C, 0x8B, 0xCB, 0x41, 0xB0, 0x01
    };
    const uint8_t context_write_2185963_mask[64] = {
        1,0,0,0,0,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_write_2185d00[64] = {
        0x00, 0x48, 0x8B, 0x9C, 0x24, 0xA0, 0x00, 0x00,
        0x00, 0x48, 0x8D, 0x4F, 0x18, 0x4C, 0x8B, 0xCB,
        0xC6, 0x87, 0xA8, 0x08, 0x00, 0x00, 0x00, 0x45,
        0x33, 0xC0, 0xC6, 0x44, 0x24, 0x20, 0x00, 0xBA,
        0x2C, 0x01, 0x00, 0x00, 0xE8, 0xC7, 0xE7, 0xFF,
        0xFF, 0x41, 0xB0, 0x01, 0x48, 0x8D, 0x8F, 0xF8,
        0x00, 0x00, 0x00, 0x41, 0x0F, 0xB6, 0xD0, 0x4C,
        0x8B, 0xCB, 0xE8, 0x61, 0x6F, 0xFF, 0xFF, 0x49
    };
    const uint8_t context_write_2185d00_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,0,0,0,
        0,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,0,0,0,0,1
    };
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract dynamic zero branch 6331F5",
        OOTP27_NO_MINOR_CONTRACT_DYNAMIC_ZERO_6331F5_RVA,
        expected_dynamic_zero_branch,
        patch_dynamic_zero_branch,
        sizeof(expected_dynamic_zero_branch),
        context_dynamic_zero_branch,
        context_dynamic_zero_branch_mask,
        sizeof(context_dynamic_zero_branch),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract dynamic reset 633286",
        OOTP27_NO_MINOR_CONTRACT_DYNAMIC_RESET_633286_RVA,
        expected_dynamic_reset,
        patch_dynamic_reset,
        sizeof(expected_dynamic_reset),
        context_dynamic_reset,
        context_dynamic_reset_mask,
        sizeof(context_dynamic_reset),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_pattern(
        exe,
        "KBO no-minor-contract offer major flag copy 13B0235",
        OOTP27_NO_MINOR_CONTRACT_OFFER_MAJOR_FLAG_COPY_13B0235_RVA,
        expected_offer_major_flag_copy,
        patch_offer_major_flag_copy,
        sizeof(expected_offer_major_flag_copy));
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer major flag reset 13B03CE",
        OOTP27_NO_MINOR_CONTRACT_OFFER_MAJOR_FLAG_RESET_13B03CE_RVA,
        expected_offer_major_flag_reset,
        patch_offer_major_flag_reset,
        sizeof(expected_offer_major_flag_reset),
        context_offer_major_flag_reset,
        context_offer_major_flag_reset_mask,
        sizeof(context_offer_major_flag_reset),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract option text branch 13E800B",
        OOTP27_NO_MINOR_CONTRACT_OPTION_TEXT_BRANCH_13E800B_RVA,
        expected_option_text_branch,
        patch_option_text_branch,
        sizeof(expected_option_text_branch),
        context_option_text_branch,
        context_option_text_branch_mask,
        sizeof(context_option_text_branch),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer UI skip minor item 17A93DF",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_MINOR_ITEM_17A93DF_RVA,
        expected_offer_ui_minor_item,
        patch_offer_ui_minor_item,
        sizeof(expected_offer_ui_minor_item),
        context_offer_ui_minor_item_17a93df,
        context_offer_ui_minor_item_17a93df_mask,
        sizeof(context_offer_ui_minor_item_17a93df),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer UI skip option item 17A9467",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_OPTION_ITEM_17A9467_RVA,
        expected_offer_ui_option_item,
        patch_offer_ui_option_item,
        sizeof(expected_offer_ui_option_item),
        context_offer_ui_option_item_17a9467,
        context_offer_ui_option_item_17a9467_mask,
        sizeof(context_offer_ui_option_item_17a9467),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer UI force selected major 17A94A0",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_SELECTION_VALUE_17A94A0_RVA,
        expected_offer_ui_selection_value,
        patch_offer_ui_selection_value,
        sizeof(expected_offer_ui_selection_value),
        context_offer_ui_selection_value_17a94a0,
        context_offer_ui_selection_value_17a94a0_mask,
        sizeof(context_offer_ui_selection_value_17a94a0),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer UI force selection branch 17A94A5",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_SELECTION_BRANCH_17A94A5_RVA,
        expected_offer_ui_selection_branch,
        patch_offer_ui_selection_branch,
        sizeof(expected_offer_ui_selection_branch),
        context_offer_ui_selection_branch_17a94a5,
        context_offer_ui_selection_branch_17a94a5_mask,
        sizeof(context_offer_ui_selection_branch_17a94a5),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract report skip minor item 17DA0EC",
        OOTP27_NO_MINOR_CONTRACT_OFFER_REPORT_MINOR_ITEM_17DA0EC_RVA,
        expected_offer_report_minor_item,
        patch_offer_report_minor_item,
        sizeof(expected_offer_report_minor_item),
        context_offer_report_minor_item_17da0ec,
        context_offer_report_minor_item_17da0ec_mask,
        sizeof(context_offer_report_minor_item_17da0ec),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract report skip option item 17DA2C6",
        OOTP27_NO_MINOR_CONTRACT_OFFER_REPORT_OPTION_ITEM_17DA2C6_RVA,
        expected_offer_report_option_item,
        patch_offer_report_option_item,
        sizeof(expected_offer_report_option_item),
        context_offer_report_option_item_17da2c6,
        context_offer_report_option_item_17da2c6_mask,
        sizeof(context_offer_report_option_item_17da2c6),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract contract type skip minor item 182666A",
        OOTP27_NO_MINOR_CONTRACT_CONTRACT_TYPE_MINOR_ITEM_182666A_RVA,
        expected_contract_type_minor_item,
        patch_contract_type_minor_item,
        sizeof(expected_contract_type_minor_item),
        context_contract_type_minor_item_182666a,
        context_contract_type_minor_item_182666a_mask,
        sizeof(context_contract_type_minor_item_182666a),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_pattern(
        exe,
        "KBO no-minor-contract block minor action case 1072DF6",
        OOTP27_NO_MINOR_CONTRACT_MINOR_ACTION_CASE_1072DF6_RVA,
        expected_minor_action_case,
        patch_minor_action_case,
        sizeof(expected_minor_action_case));
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract block minor extension action case 1072F32",
        OOTP27_NO_MINOR_CONTRACT_MINOR_EXTENSION_ACTION_CASE_1072F32_RVA,
        expected_minor_extension_action_case,
        patch_minor_extension_action_case,
        sizeof(expected_minor_extension_action_case),
        context_minor_extension_action_case,
        context_minor_extension_action_case_mask,
        sizeof(context_minor_extension_action_case),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch write 638D32",
        OOTP27_NO_MINOR_CONTRACT_WRITE_638D32_RVA,
        expected_rax,
        patch_rax,
        sizeof(expected_rax),
        context_write_638d32,
        context_write_638d32_mask,
        sizeof(context_write_638d32),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_pattern_ordinal(
        exe,
        "KBO no-minor-contract experimental patch option write 6393A3",
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option),
        0,
        6);
    ok |= patch_kbo_no_minor_contract_write_site_by_pattern_ordinal(
        exe,
        "KBO no-minor-contract experimental patch option write 6396D4",
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option),
        1,
        6);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch write 6DE7C1",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE7C1_RVA,
        expected_rax,
        patch_rax,
        sizeof(expected_rax),
        context_write_6de7c1,
        context_write_6de7c1_mask,
        sizeof(context_write_6de7c1),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch option write 6DDEA2",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DDEA2_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option),
        context_write_6ddea2,
        context_write_6ddea2_mask,
        sizeof(context_write_6ddea2),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch option write 6DE33E",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE33E_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option),
        context_write_6de33e,
        context_write_6de33e_mask,
        sizeof(context_write_6de33e),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch option write 6DE58C",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE58C_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option),
        context_write_6de58c,
        context_write_6de58c_mask,
        sizeof(context_write_6de58c),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch option write 6DE6BF",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE6BF_RVA,
        expected_rax_option,
        patch_rax,
        sizeof(expected_rax_option),
        context_write_6de6bf,
        context_write_6de6bf_mask,
        sizeof(context_write_6de6bf),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_pattern(
        exe,
        "KBO no-minor-contract experimental patch option-alt write 6DE96E",
        OOTP27_NO_MINOR_CONTRACT_WRITE_6DE96E_RVA,
        expected_rax_option_alt,
        patch_rax,
        sizeof(expected_rax_option_alt));
    ok |= patch_kbo_no_minor_contract_write_site_by_pattern_ordinal(
        exe,
        "KBO no-minor-contract experimental patch write 5568EF",
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi),
        0,
        5);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch write A4BE04",
        OOTP27_NO_MINOR_CONTRACT_WRITE_A4BE04_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi),
        context_write_a4be04,
        context_write_a4be04_mask,
        sizeof(context_write_a4be04),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch write 2184B61",
        OOTP27_NO_MINOR_CONTRACT_WRITE_2184B61_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi),
        context_write_2184b61,
        context_write_2184b61_mask,
        sizeof(context_write_2184b61),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch write 2185963",
        OOTP27_NO_MINOR_CONTRACT_WRITE_2185963_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi),
        context_write_2185963,
        context_write_2185963_mask,
        sizeof(context_write_2185963),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract experimental patch write 2185D00",
        OOTP27_NO_MINOR_CONTRACT_WRITE_2185D00_RVA,
        expected_rdi,
        patch_rdi,
        sizeof(expected_rdi),
        context_write_2185d00,
        context_write_2185d00_mask,
        sizeof(context_write_2185d00),
        16u);
    ok |= install_kbo_no_minor_contract_scan_patch(exe);
    ok |= install_kbo_no_minor_contract_string_patch(exe);

    append_logf("KBO no-minor-contract experimental patch complete installed_any=%d", ok);
    return ok;
}

