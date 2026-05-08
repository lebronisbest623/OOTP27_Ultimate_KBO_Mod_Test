#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_log.h"
#include "../../fa_filing/fa_filing.h"
#include "../../foreign/foreign_waiver_date.h"
#include "../../foreign/signability/submit_offer_probe_state.h"
#include "../../hook_stubs/hook_stubs_military.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"
#include "arbitration_no_withdraw_patch.h"
#include "arbitration_offer_floor_stubs.h"
#include "arbitration_patch_helpers.h"
typedef void (__fastcall *OotpArbitrationNonTenderFn)(void* team, void* player, uint8_t notify);
typedef uint8_t* (__fastcall *KboArbitrationLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);

static int32_t kbo_salary_arbitration_resolve_minimum_salary(uint32_t league_id)
{
    if (league_id == 0u || league_id > 100000u) {
        int32_t current_floor = kbo_no_minor_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    int32_t current_floor = kbo_no_minor_current_league_minimum_salary();
    return current_floor > 0 ? current_floor : 35600;
}

static uintptr_t kbo_salary_arbitration_caller_rva(void* return_address)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL || return_address == NULL) {
        return 0;
    }
    return (uintptr_t)return_address - (uintptr_t)exe;
}

static int kbo_salary_arbitration_is_known_non_tender_return(uintptr_t caller_rva)
{
    return caller_rva == 0x0067C0F1u
        || caller_rva == 0x00681B6Bu
        || caller_rva == 0x006820C6u;
}

__declspec(noinline) void ootp_kbo_salary_arbitration_non_tender_wrapper(
    void* team_ptr,
    void* player_ptr,
    uint8_t notify,
    OotpArbitrationNonTenderFn original_func)
{
    if (player_ptr == NULL || !memory_range_readable(player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        return;
    }

    void* return_address = __builtin_return_address(0);
    uintptr_t caller_rva = kbo_salary_arbitration_caller_rva(return_address);
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t pre_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t pre_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t player_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t player_draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint32_t team_league_id = 0u;
    uint32_t team_id = 0u;
    if (team_ptr != NULL
            && memory_range_readable(
                (uint8_t*)team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET,
                OOTP27_KBO_TEAM_ID_OFFSET - OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET + sizeof(uint32_t))) {
        uint8_t* team = (uint8_t*)team_ptr;
        team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    }

    int kbo_context =
        team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
        || player_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
        || player_draft_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
    if (!kbo_context) {
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        return;
    }

    uint8_t contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    int32_t old_offer = *(int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET);
    uint32_t original_team_id = team_id != 0u
        ? team_id
        : (pre_active_team_id != 0u ? pre_active_team_id : pre_current_team_id);
    uint32_t original_team_league_id = team_league_id != 0u
        ? team_league_id
        : kbo_fa_filing_team_league_id(original_team_id);
    int fa_filing_candidate =
        kbo_fa_filing_is_official_transition_caller(caller_rva)
        && notify == 0u
        && contract_level != 0u
        && nation_id == OOTP27_KBO_KOREA_NATION_ID
        && original_team_id != 0u
        && original_team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
    int direct_block_candidate = contract_level != 0u
        && old_offer <= 0
        && kbo_salary_arbitration_is_known_non_tender_return(caller_rva);
    int should_block = 0;
    if (!should_block) {
        static LONG pass_log_count = 0;
        LONG pass_slot = InterlockedIncrement(&pass_log_count);
        if (pass_slot <= 120) {
            append_logf(
                "KBO salary arbitration non-tender pass-through player=%u team=%u team_league=%u player_league=%u draft_league=%u contract_level=%u offer=%d notify=%u caller_rva=0x%llx direct_block_candidate=%d",
                player_id,
                team_id,
                team_league_id,
                player_league_id,
                player_draft_league_id,
                (unsigned)contract_level,
                old_offer,
                (unsigned)notify,
                (unsigned long long)caller_rva,
                direct_block_candidate ? 1 : 0);
        }
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        if (fa_filing_candidate && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            uint32_t post_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            if (post_current_team_id == 0u) {
                uint32_t today = 0u;
                if (kbo_get_current_yyyymmdd(&today) && today != 0u) {
                    kbo_record_fa_filing_transition(
                        (uintptr_t)player_ptr,
                        player_id,
                        today,
                        original_team_id,
                        original_team_league_id,
                        (uint32_t)caller_rva,
                        notify,
                        contract_level,
                        "arbitration_non_tender_67d11e");
                }
            }
        }
        return;
    }

    uint32_t floor_league_id = team_league_id != 0u
        ? team_league_id
        : (player_league_id != 0u ? player_league_id : player_draft_league_id);
    int32_t salary_floor = kbo_salary_arbitration_resolve_minimum_salary(floor_league_id);
    if (old_offer < salary_floor) {
        *(int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET) = salary_floor;
    }

    static LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 120) {
        append_logf(
            "KBO salary arbitration non-tender blocked player=%u team=%u team_league=%u player_league=%u draft_league=%u contract_level=%u old_offer=%d floor=%d notify=%u caller_rva=0x%llx",
            player_id,
            team_id,
            team_league_id,
            player_league_id,
            player_draft_league_id,
            (unsigned)contract_level,
            old_offer,
            salary_floor,
            (unsigned)notify,
            (unsigned long long)caller_rva);
    }
}


static uint8_t* build_kbo_salary_arbitration_non_tender_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x49, 0xB9,                                     /* mov r9, original_trampoline */
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     /* mov rax, wrapper */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_salary_arbitration_non_tender_wrapper);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

static int patch_kbo_salary_arbitration_r11_detour_at(
    const char* label,
    uint8_t* target,
    const uint8_t* expected,
    size_t size,
    uint8_t* stub);

static int patch_kbo_salary_arbitration_r11_detour_at(
    const char* label,
    uint8_t* target,
    const uint8_t* expected,
    size_t size,
    uint8_t* stub)
{
    if (label == NULL || target == NULL || expected == NULL || size < 13 || stub == NULL) {
        return 0;
    }

    if (!memory_range_readable(target, size)) {
        append_logf("%s target unreadable target=%p", label, target);
        return 0;
    }
    if (is_r11_absolute_jump_patch(target)) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, size) != 0) {
        log_patch_bytes_mismatch(label, target, size);
        return 0;
    }

    uint8_t patch[32] = {
        0x49, 0xBB,                                     /* mov r11, stub */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3                                /* jmp r11 */
    };
    for (size_t i = 13; i < size && i < sizeof(patch); i++) {
        patch[i] = 0x90;
    }
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);

    DWORD ignored = 0;
    VirtualProtect(target, size, old_protect, &ignored);

    append_logf("installed %s target=%p stub=%p", label, target, stub);
    return 1;
}

static int install_kbo_salary_arbitration_non_tender_function_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const size_t stolen_len = 18;
    const uint8_t expected[18] = {
        0x48, 0x89, 0x54, 0x24, 0x10,                   /* mov [rsp+0x10], rdx */
        0x48, 0x89, 0x4C, 0x24, 0x08,                   /* mov [rsp+0x8], rcx */
        0x55,                                           /* push rbp */
        0x53,                                           /* push rbx */
        0x56,                                           /* push rsi */
        0x57,                                           /* push rdi */
        0x41, 0x54,                                     /* push r12 */
        0x41, 0x55                                      /* push r13 */
    };
    const uint8_t context[48] = {
        0x83, 0x68, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3, 0xCC,
        0x48, 0x89, 0x54, 0x24, 0x10, 0x48, 0x89, 0x4C,
        0x24, 0x08, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54,
        0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D,
        0x6C, 0x24, 0xE1, 0x48, 0x81, 0xEC, 0x88, 0x00
    };

    uint8_t* target = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_ARBITRATION_NON_TENDER_FUNCTION_RVA,
        expected,
        sizeof(expected),
        context,
        sizeof(context),
        16u,
        "KBO salary arbitration non-tender function");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO salary arbitration non-tender function already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO salary arbitration non-tender trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_salary_arbitration_non_tender_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO salary arbitration non-tender detour stub");
        return 0;
    }

    uint8_t patch[18] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO salary arbitration non-tender function error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO salary arbitration non-tender function target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_salary_arbitration_non_tender_wrapper);
    return 1;
}


int install_kbo_salary_arbitration_no_withdraw_patch(void)
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
    const uint8_t context_ui_hide_branch[48] = {
        0x49, 0x8B, 0xCD, 0xE8, 0x26, 0x5E, 0xAF, 0x00,
        0x41, 0x83, 0xBF, 0x7C, 0x08, 0x00, 0x00, 0x00,
        0x0F, 0x85, 0x8D, 0x01, 0x00, 0x00, 0x80, 0xBB,
        0x35, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x84, 0x80,
        0x01, 0x00, 0x00, 0x80, 0x7B, 0x15, 0x00, 0x74,
        0x0C, 0x48, 0x8B, 0x03, 0x48, 0x8B, 0xCB, 0xFF
    };
    const uint8_t context_ui_hide_branch_mask[48] = {
        1,1,1,1,0,0,0,0, 1,1,1,1,1,1,1,1,
        1,1,0,0,0,0, 1,1,1,1,1,1,1,1,1,0,
        0,0,0,1, 1,1,1,1,1,1,1,1,1,1,1,1
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
    const uint8_t context_final_zero_tender_gate[48] = {
        0x49, 0x8B, 0xCE, 0xE8, 0x08, 0x32, 0x3B, 0x00,
        0x84, 0xC0, 0x0F, 0x85, 0xE6, 0x00, 0x00, 0x00,
        0x41, 0x83, 0xBD, 0x7C, 0x08, 0x00, 0x00, 0x00,
        0x0F, 0x8F, 0xD8, 0x00, 0x00, 0x00, 0x49, 0x8B,
        0xD5, 0x49, 0x8B, 0xCC, 0xE8, 0x67, 0x1C, 0xFF,
        0xFF, 0x84, 0xC0, 0x75, 0x20, 0x41, 0x88, 0x85
    };
    const uint8_t context_final_zero_tender_gate_mask[48] = {
        1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 1,1,0,0,0,0,1,1,
        1,1,1,1,1,0,0,0, 0,1,1,1,1,1,1,1
    };

    const uint8_t expected_ai_offer_write_681012[14] = {
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], eax */
        0x66, 0x41, 0x0F, 0x2F, 0xF3,                   /* comisd xmm6, xmm11 */
        0x76, 0x2A                                      /* jbe 0x14068204a */
    };
    const uint8_t context_ai_offer_write_681012[48] = {
        0x78, 0x3B, 0xC8, 0x7D, 0x0B, 0x89, 0x85, 0x28,
        0x01, 0x00, 0x00, 0x8B, 0xC1, 0x89, 0x4D, 0x78,
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00, 0x66,
        0x41, 0x0F, 0x2F, 0xF3, 0x76, 0x2A, 0x49, 0x8D,
        0x9D, 0xA0, 0x08, 0x00, 0x00, 0x48, 0x8D, 0x95,
        0xF0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCB, 0xE8
    };

    const uint8_t expected_zero_offer_check_682089[18] = {
        0x41, 0x8B, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov eax, [r13+0x87c] */
        0x85, 0xC0,                                     /* test eax, eax */
        0x74, 0x23,                                     /* je 0x1406820b7 */
        0x41, 0x8B, 0x8D, 0x80, 0x08, 0x00, 0x00        /* mov ecx, [r13+0x880] */
    };
    const uint8_t context_zero_offer_check_682089[48] = {
        0x49, 0x8D, 0xBD, 0x14, 0x09, 0x00, 0x00, 0x48,
        0x8B, 0x75, 0xB8, 0xE9, 0x76, 0x01, 0x00, 0x00,
        0x41, 0x8B, 0x85, 0x7C, 0x08, 0x00, 0x00, 0x85,
        0xC0, 0x74, 0x23, 0x41, 0x8B, 0x8D, 0x80, 0x08,
        0x00, 0x00, 0x3B, 0xC1, 0x0F, 0x8C, 0xCD, 0x00,
        0x00, 0x00, 0x48, 0x8D, 0x95, 0xA0, 0x01, 0x00
    };

    const uint8_t expected_high_limit_skip_rsi_branch[6] = {
        0x0F, 0x8F, 0xDE, 0x02, 0x00, 0x00              /* jg 0x1406820b3 */
    };
    const uint8_t patch_high_limit_skip_rsi_branch[6] = {
        0x0F, 0x8F, 0xDA, 0x02, 0x00, 0x00              /* jg 0x1406820af */
    };
    const uint8_t context_high_limit_skip_rsi_branch[48] = {
        0xEB, 0x02, 0x8B, 0xCF, 0x41, 0x8B, 0x84, 0x24,
        0x98, 0x02, 0x00, 0x00, 0x03, 0xC0, 0x3B, 0xC8,
        0x0F, 0x8F, 0xDE, 0x02, 0x00, 0x00, 0x84, 0xDB,
        0x74, 0x47, 0x33, 0xD2, 0x48, 0x8D, 0x4D, 0x40,
        0xE8, 0x0C, 0x34, 0x22, 0x00, 0x66, 0x0F, 0x6E,
        0xC8, 0xF3, 0x0F, 0xE6, 0xC9, 0x66, 0x0F, 0x6E
    };
    const uint8_t context_high_limit_skip_rsi_branch_mask[48] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,0,0,0,0, 1,1,1,1,1,1,1,1,1,1,
        1,0,0,0,0, 1,1,1,1,1,1,1,1,1,1,1
    };

    const uint8_t expected_high_limit_non_tender_gate_6820af[18] = {
        0x48, 0x8B, 0x75, 0xB8,                         /* mov rsi, [rbp-0x48] */
        0x4C, 0x8B, 0x75, 0xC0,                         /* mov r14, [rbp-0x40] */
        0x45, 0x0F, 0xB6, 0xC7,                         /* movzx r8d, r15b */
        0x49, 0x8B, 0xD5,                               /* mov rdx, r13 */
        0x49, 0x8B, 0xCE                                /* mov rcx, r14 */
    };
    const uint8_t context_high_limit_non_tender_gate_6820af[48] = {
        0xCD, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x95, 0xA0,
        0x01, 0x00, 0x00, 0xE9, 0x41, 0x01, 0x00, 0x00,
        0x48, 0x8B, 0x75, 0xB8, 0x4C, 0x8B, 0x75, 0xC0,
        0x45, 0x0F, 0xB6, 0xC7, 0x49, 0x8B, 0xD5, 0x49,
        0x8B, 0xCE, 0xE8, 0xAA, 0x39, 0x3D, 0x00, 0x41,
        0xC6, 0x85, 0xB0, 0x0D, 0x00, 0x00, 0x00, 0x48
    };
    const uint8_t context_high_limit_non_tender_gate_6820af_mask[48] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,0,0,0,0,1, 1,1,1,1,1,1,1,1
    };

    const uint8_t expected_ai_offer_write_6827cd[13] = {
        0x89, 0x83, 0x7C, 0x08, 0x00, 0x00,             /* mov [rbx+0x87c], eax */
        0x48, 0x8D, 0x05, 0xFE, 0x23, 0x43, 0x02        /* lea 0x142ab4bd8, rax */
    };
    const uint8_t context_ai_offer_write_6827cd[48] = {
        0x45, 0x80, 0x48, 0x8B, 0xD3, 0x49, 0x8B, 0xCF,
        0xE8, 0xB6, 0xC7, 0xFF, 0xFF, 0x8B, 0x45, 0xB8,
        0x89, 0x83, 0x7C, 0x08, 0x00, 0x00, 0x48, 0x8D,
        0x05, 0xFE, 0x23, 0x43, 0x02, 0x48, 0x89, 0x45,
        0x80, 0x48, 0x8D, 0x55, 0x18, 0x48, 0x8D, 0x4D,
        0x18, 0xE8, 0x75, 0xFA, 0xEE, 0xFF, 0xE9, 0xEA
    };
    const uint8_t context_ai_offer_write_6827cd_mask[48] = {
        1,1,1,1,1,1,1,1, 1,0,0,0,0,1,1,1,
        1,1,1,1,1,1,1,1, 1,0,0,0,0,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,0,0,0,0,1,1
    };

    int ok = 0;
    uint8_t* ui_hide_branch_target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_WITHDRAW_UI_HIDE_BRANCH_RVA,
        expected_ui_hide_branch,
        sizeof(expected_ui_hide_branch),
        context_ui_hide_branch,
        context_ui_hide_branch_mask,
        sizeof(context_ui_hide_branch),
        16u,
        "KBO salary arbitration withdraw UI hide branch");
    if (ui_hide_branch_target != NULL) {
        ok |= patch_static_bytes(
            "KBO salary arbitration withdraw UI hide branch",
            ui_hide_branch_target,
            expected_ui_hide_branch,
            patch_ui_hide_branch,
            sizeof(expected_ui_hide_branch));
    }
    uint8_t* action_gate_target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_ARBITRATION_WITHDRAW_ACTION_GATE_RVA,
        expected_action_gate,
        sizeof(expected_action_gate),
        "KBO salary arbitration withdraw action gate");
    if (action_gate_target != NULL) {
        ok |= patch_static_bytes(
            "KBO salary arbitration withdraw action gate",
            action_gate_target,
            expected_action_gate,
            patch_action_gate,
            sizeof(expected_action_gate));
    }

    uint8_t* final_zero_target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_GATE_RVA,
        expected_final_zero_tender_gate,
        sizeof(expected_final_zero_tender_gate),
        context_final_zero_tender_gate,
        context_final_zero_tender_gate_mask,
        sizeof(context_final_zero_tender_gate),
        16u,
        "KBO salary arbitration AI final zero-offer tender floor");
    if (final_zero_target != NULL) {
        uint8_t* final_zero_stub = build_kbo_salary_arbitration_final_zero_tender_stub(
            final_zero_target + sizeof(expected_final_zero_tender_gate),
            final_zero_target + (OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_CONTINUE_RVA - OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_GATE_RVA));
        if (final_zero_stub != NULL) {
            ok |= patch_kbo_salary_arbitration_r11_detour_at(
                "KBO salary arbitration AI final zero-offer tender floor",
                final_zero_target,
                expected_final_zero_tender_gate,
                sizeof(expected_final_zero_tender_gate),
                final_zero_stub);
        } else {
            append_log_line("failed to allocate KBO salary arbitration final zero-offer tender floor stub");
        }
    } else {
        append_log_line("KBO salary arbitration AI final zero-offer tender floor target unresolved");
    }

    uint8_t* ai_offer_target_681012 = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_RVA,
        expected_ai_offer_write_681012,
        sizeof(expected_ai_offer_write_681012),
        context_ai_offer_write_681012,
        sizeof(context_ai_offer_write_681012),
        16u,
        "KBO salary arbitration AI offer floor 681012");
    if (ai_offer_target_681012 != NULL) {
        uint8_t* ai_offer_stub_681012 = build_kbo_salary_arbitration_ai_offer_write_681012_stub(
            ai_offer_target_681012 + (OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_FLOOR_RETURN_RVA - OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_RVA),
            ai_offer_target_681012 + (OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_PASS_RETURN_RVA - OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_RVA));
        if (ai_offer_stub_681012 != NULL) {
            ok |= patch_kbo_salary_arbitration_r11_detour_at(
            "KBO salary arbitration AI offer floor 681012",
            ai_offer_target_681012,
            expected_ai_offer_write_681012,
            sizeof(expected_ai_offer_write_681012),
            ai_offer_stub_681012);
        } else {
            append_log_line("failed to allocate KBO salary arbitration AI offer floor 681012 stub");
        }
    } else {
        append_log_line("KBO salary arbitration AI offer floor 681012 target unresolved");
    }

    uint8_t* zero_offer_target_682089 = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_RVA,
        expected_zero_offer_check_682089,
        sizeof(expected_zero_offer_check_682089),
        context_zero_offer_check_682089,
        sizeof(context_zero_offer_check_682089),
        16u,
        "KBO salary arbitration AI zero-offer final check 682089");
    if (zero_offer_target_682089 != NULL) {
        uint8_t* zero_offer_check_stub_682089 = build_kbo_salary_arbitration_zero_offer_check_682089_stub(
            zero_offer_target_682089 + (OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_TENDER_RETURN_RVA - OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_RVA),
            zero_offer_target_682089 + (OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_PASS_RETURN_RVA - OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_RVA));
        if (zero_offer_check_stub_682089 != NULL) {
            ok |= patch_kbo_salary_arbitration_r11_detour_at(
            "KBO salary arbitration AI zero-offer final check 682089",
            zero_offer_target_682089,
            expected_zero_offer_check_682089,
            sizeof(expected_zero_offer_check_682089),
            zero_offer_check_stub_682089);
        } else {
            append_log_line("failed to allocate KBO salary arbitration AI zero-offer final check 682089 stub");
        }
    } else {
        append_log_line("KBO salary arbitration AI zero-offer final check 682089 target unresolved");
    }

    uint8_t* high_limit_skip_target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_HIGH_LIMIT_SKIP_RSI_BRANCH_RVA,
        expected_high_limit_skip_rsi_branch,
        sizeof(expected_high_limit_skip_rsi_branch),
        context_high_limit_skip_rsi_branch,
        context_high_limit_skip_rsi_branch_mask,
        sizeof(context_high_limit_skip_rsi_branch),
        16u,
        "KBO salary arbitration high-limit skip-rsi branch retarget");
    if (high_limit_skip_target != NULL) {
        ok |= patch_static_bytes(
            "KBO salary arbitration high-limit skip-rsi branch retarget",
            high_limit_skip_target,
            expected_high_limit_skip_rsi_branch,
            patch_high_limit_skip_rsi_branch,
            sizeof(expected_high_limit_skip_rsi_branch));
    }

    uint8_t* high_limit_non_tender_target_6820af = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_GATE_RVA,
        expected_high_limit_non_tender_gate_6820af,
        sizeof(expected_high_limit_non_tender_gate_6820af),
        context_high_limit_non_tender_gate_6820af,
        context_high_limit_non_tender_gate_6820af_mask,
        sizeof(context_high_limit_non_tender_gate_6820af),
        16u,
        "KBO salary arbitration high-limit non-tender gate 6820af");
    if (high_limit_non_tender_target_6820af != NULL) {
        uint8_t* high_limit_non_tender_stub_6820af = build_kbo_salary_arbitration_high_limit_non_tender_6820af_stub(
            high_limit_non_tender_target_6820af + (OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_6820AF_PASS_RETURN_RVA - OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_GATE_RVA),
            high_limit_non_tender_target_6820af + (OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_6820AF_FLOOR_RETURN_RVA - OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_GATE_RVA));
        if (high_limit_non_tender_stub_6820af != NULL) {
            ok |= patch_kbo_salary_arbitration_r11_detour_at(
                "KBO salary arbitration high-limit non-tender gate 6820af",
                high_limit_non_tender_target_6820af,
                expected_high_limit_non_tender_gate_6820af,
                sizeof(expected_high_limit_non_tender_gate_6820af),
                high_limit_non_tender_stub_6820af);
        } else {
            append_log_line("failed to allocate KBO salary arbitration high-limit non-tender gate 6820af stub");
        }
    } else {
        append_log_line("KBO salary arbitration high-limit non-tender gate 6820af target unresolved");
    }

    uint8_t* ai_offer_target_6827cd = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RVA,
        expected_ai_offer_write_6827cd,
        sizeof(expected_ai_offer_write_6827cd),
        context_ai_offer_write_6827cd,
        context_ai_offer_write_6827cd_mask,
        sizeof(context_ai_offer_write_6827cd),
        16u,
        "KBO salary arbitration AI offer floor 6827cd");
    if (ai_offer_target_6827cd != NULL) {
        void* ai_offer_superstar_source_6827cd = resolve_rip_relative_lea_target(ai_offer_target_6827cd + 6);
        if (ai_offer_superstar_source_6827cd == NULL) {
            append_log_line("failed to resolve KBO salary arbitration AI offer floor 6827cd superstar source");
        } else {
            uint8_t* ai_offer_stub_6827cd = build_kbo_salary_arbitration_ai_offer_write_6827cd_stub(
                ai_offer_superstar_source_6827cd,
                ai_offer_target_6827cd + (OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RETURN_RVA - OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RVA));
            if (ai_offer_stub_6827cd != NULL) {
                ok |= patch_kbo_salary_arbitration_r11_detour_at(
                    "KBO salary arbitration AI offer floor 6827cd",
                    ai_offer_target_6827cd,
                    expected_ai_offer_write_6827cd,
                    sizeof(expected_ai_offer_write_6827cd),
                    ai_offer_stub_6827cd);
            } else {
                append_log_line("failed to allocate KBO salary arbitration AI offer floor 6827cd stub");
            }
        }
    } else {
        append_log_line("KBO salary arbitration AI offer floor 6827cd target unresolved");
    }

    ok |= install_kbo_salary_arbitration_non_tender_function_patch(exe);

    append_logf("KBO salary arbitration no-withdraw/zero-offer patch complete installed_any=%d", ok ? 1 : 0);
    return ok;
}

