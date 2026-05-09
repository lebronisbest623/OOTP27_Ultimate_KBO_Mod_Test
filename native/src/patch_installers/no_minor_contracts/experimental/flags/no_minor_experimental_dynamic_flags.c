#include "../internal/no_minor_experimental_patch_internal.h"

int install_kbo_no_minor_contract_dynamic_flag_patches(HMODULE exe)
{
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

    int ok = 0;
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
    return ok;
}
