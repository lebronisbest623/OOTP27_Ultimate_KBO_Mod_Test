#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../hook_stubs/allstar/candidate/hook_stubs_allstar_candidate.h"
#include "../../../hook_stubs/allstar/events/hook_stubs_allstar_events.h"
#include "../../../hook_stubs/allstar/settings/hook_stubs_allstar_settings.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../common/patch_installers_allstar_common.h"
#include "patch_installers_allstar_candidate.h"

typedef struct AllstarRankedCandidatePushSite {
    const char* label;
    uint32_t site_rva;
    uint32_t skip_rva;
    int vector_base_in_rax;
} AllstarRankedCandidatePushSite;

int install_allstar_candidate_team_split_patch(void)
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

    const uint8_t* expected = april_expected;
    uint8_t* target = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    if (target != NULL) {
        expected = may_expected;
    } else {
        target = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }

    if (target == NULL || !memory_range_readable(target, sizeof(april_expected))) {
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
            append_log_line("KBO all-star candidate team split hook skipped: vector push helper target unresolved");
            return 0;
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

int install_allstar_candidate_team_roster_push_filter_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star team-roster candidate push filter patch");
    if (exe == NULL) {
        return 0;
    }

    const uint8_t expected[OOTP27_ALLSTAR_CANDIDATE_TEAM_ROSTER_PUSH_STOLEN_LEN] = {
        0x49, 0x8B, 0xCE,
        0xE8, 0xEA, 0x36, 0x5E, 0x01,
        0xFF, 0xC7,
        0x48, 0xFF, 0xC3,
        0x49, 0x8B, 0xCF
    };

    uint8_t* rva_target = (uint8_t*)exe + OOTP27_ALLSTAR_CANDIDATE_TEAM_ROSTER_PUSH_SITE_RVA;
    uint8_t* target = NULL;
    if (memory_range_readable(rva_target, sizeof(expected))
            && (is_rip_absolute_jump_patch(rva_target) || is_rax_absolute_jump_patch(rva_target))) {
        append_logf("KBO all-star team-roster candidate push filter hook already installed target=%p", rva_target);
        return 1;
    }
    if (memory_range_readable(rva_target, sizeof(expected)) && memcmp(rva_target, expected, sizeof(expected)) == 0) {
        target = rva_target;
    } else {
        target = find_ootp_executable_pattern(expected, sizeof(expected));
        if (target != NULL) {
            append_logf("KBO all-star team-roster candidate push filter target resolved by pattern target=%p", target);
        }
    }

    if (target == NULL || !memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO all-star team-roster candidate push filter hook target unreadable target=%p", target);
        return 0;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star team-roster candidate push filter hook", target, sizeof(expected));
        log_extended_context("KBO all-star team-roster candidate push filter hook", target, 16, 80);
        return 0;
    }

    void* vector_push_back_address = resolve_relative_call_target(
        target + OOTP27_ALLSTAR_CANDIDATE_TEAM_ROSTER_PUSH_CALL_OFFSET);
    if (vector_push_back_address == NULL) {
        append_log_line("KBO all-star team-roster candidate push filter hook skipped: vector push helper target unresolved");
        return 0;
    }

    void* return_address = target + OOTP27_ALLSTAR_CANDIDATE_TEAM_ROSTER_PUSH_STOLEN_LEN;
    uint8_t* stub = build_allstar_candidate_team_roster_push_filter_stub(
        return_address,
        vector_push_back_address);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO all-star team-roster candidate push filter hook stub");
        return 0;
    }

    uint8_t patch[OOTP27_ALLSTAR_CANDIDATE_TEAM_ROSTER_PUSH_STOLEN_LEN] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0,
        0x90, 0x90
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO all-star team-roster candidate push filter hook error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);
    append_logf(
        "installed KBO all-star team-roster candidate push filter hook target=%p stub=%p return=%p vector_push=%p helper=%p",
        target,
        stub,
        return_address,
        vector_push_back_address,
        &ootp_kbo_allstar_candidate_push_filter);
    return 1;
}

static int ranked_player_push_bytes_match(uint8_t* target, int vector_base_in_rax)
{
    const uint8_t r8_prefix[14] = {
        0x8B, 0x4C, 0x24, 0x48,
        0x48, 0xC1, 0xE1, 0x05,
        0x49, 0x03, 0xC8,
        0x48, 0x8B, 0xD3
    };
    const uint8_t rax_prefix[14] = {
        0x8B, 0x4C, 0x24, 0x48,
        0x48, 0xC1, 0xE1, 0x05,
        0x48, 0x03, 0xC8,
        0x48, 0x8B, 0xD3
    };
    const uint8_t suffix[7] = {
        0x44, 0x89, 0xAB, 0xE8, 0x0F, 0x00, 0x00
    };
    const uint8_t* prefix = vector_base_in_rax ? rax_prefix : r8_prefix;

    return memcmp(target, prefix, sizeof(r8_prefix)) == 0
        && target[OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_CALL_OFFSET] == 0xE8
        && memcmp(
            target + OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_CALL_OFFSET + 5u,
            suffix,
            sizeof(suffix)) == 0;
}

static int install_ranked_player_push_filter_site(HMODULE exe, const AllstarRankedCandidatePushSite* site)
{
    uint8_t* target = (uint8_t*)exe + site->site_rva;
    if (memory_range_readable(target, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN)
            && (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target))) {
        append_logf(
            "KBO all-star ranked candidate player push filter hook already installed label=%s target=%p",
            site->label,
            target);
        return 1;
    }

    if (!memory_range_readable(target, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN)) {
        append_logf(
            "KBO all-star ranked candidate player push filter hook target unreadable label=%s target=%p",
            site->label,
            target);
        return 0;
    }

    if (!ranked_player_push_bytes_match(target, site->vector_base_in_rax)) {
        append_logf(
            "KBO all-star ranked candidate player push filter hook byte mismatch label=%s target=%p",
            site->label,
            target);
        log_patch_bytes_mismatch(
            "KBO all-star ranked candidate player push filter hook",
            target,
            OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN);
        log_extended_context("KBO all-star ranked candidate player push filter hook", target, 16, 80);
        return 0;
    }

    void* vector_push_back_address = resolve_relative_call_target(
        target + OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_CALL_OFFSET);
    if (vector_push_back_address == NULL) {
        append_logf(
            "KBO all-star ranked candidate player push filter hook skipped: vector push helper target unresolved label=%s",
            site->label);
        return 0;
    }

    void* return_address = target + OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN;
    void* skip_address = (uint8_t*)exe + site->skip_rva;
    uint8_t* stub = build_allstar_candidate_ranked_player_push_filter_stub(
        return_address,
        skip_address,
        vector_push_back_address,
        site->vector_base_in_rax);
    if (stub == NULL) {
        append_logf(
            "failed to allocate KBO all-star ranked candidate player push filter hook stub label=%s",
            site->label);
        return 0;
    }

    uint8_t patch[OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0,
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf(
            "VirtualProtect failed for KBO all-star ranked candidate player push filter hook label=%s error=%lu",
            site->label,
            GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);
    append_logf(
        "installed KBO all-star ranked candidate player push filter hook label=%s target=%p stub=%p return=%p skip=%p vector_push=%p helper=%p base=%s",
        site->label,
        target,
        stub,
        return_address,
        skip_address,
        vector_push_back_address,
        &ootp_kbo_allstar_candidate_push_filter,
        site->vector_base_in_rax ? "rax" : "r8");
    return 1;
}

int install_allstar_candidate_ranked_player_push_filter_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star ranked candidate player push filter patch");
    if (exe == NULL) {
        return 0;
    }

    const AllstarRankedCandidatePushSite sites[] = {
        { "quota-catcher", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_1_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "quota-infield", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_2_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "quota-dh", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_3_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "quota-outfield", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_4_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "quota-pitcher", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_5_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "fallback-fill", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_6_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_FALLBACK_SKIP_RVA, 1 }
    };

    int ok = 1;
    int installed = 0;
    for (int i = 0; i < (int)(sizeof(sites) / sizeof(sites[0])); i++) {
        int site_ok = install_ranked_player_push_filter_site(exe, &sites[i]);
        if (!site_ok) {
            ok = 0;
        } else {
            installed++;
        }
    }

    append_logf(
        "KBO all-star ranked candidate player push filter hooks finished ok=%d installed_or_present=%d total=%d",
        ok,
        installed,
        (int)(sizeof(sites) / sizeof(sites[0])));
    return ok;
}

int install_allstar_candidate_player_push_filter_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star candidate player push filter patch");
    if (exe == NULL) {
        return 0;
    }

    const uint8_t expected[OOTP27_ALLSTAR_CANDIDATE_PLAYER_PUSH_STOLEN_LEN] = {
        0x49, 0x8B, 0xD6,
        0xE8, 0xA1, 0x34, 0x5E, 0x01,
        0x8B, 0xC3,
        0x44, 0x8B, 0x64, 0x24, 0x58
    };
    const uint8_t context[32] = {
        0x8B, 0x4C, 0x24, 0x48,
        0x48, 0xC1, 0xE1, 0x05,
        0x48, 0x03, 0xCE,
        0x49, 0x8B, 0xD6,
        0xE8, 0xA1, 0x34, 0x5E, 0x01,
        0x8B, 0xC3,
        0x44, 0x8B, 0x64, 0x24, 0x58,
        0x45, 0x85, 0xE4,
        0x0F, 0x94, 0xC0
    };
    const size_t context_target_offset = 11u;

    uint8_t* rva_target = (uint8_t*)exe + OOTP27_ALLSTAR_CANDIDATE_PLAYER_PUSH_SITE_RVA;
    uint8_t* target = NULL;
    if (memory_range_readable(rva_target, sizeof(expected))
            && (is_rip_absolute_jump_patch(rva_target) || is_rax_absolute_jump_patch(rva_target))) {
        append_logf("KBO all-star candidate player push filter hook already installed target=%p", rva_target);
        return 1;
    }
    if (memory_range_readable(rva_target, sizeof(expected)) && memcmp(rva_target, expected, sizeof(expected)) == 0) {
        target = rva_target;
    } else {
        uint8_t* context_base = find_ootp_executable_pattern(context, sizeof(context));
        if (context_base != NULL) {
            target = context_base + context_target_offset;
            append_logf(
                "KBO all-star candidate player push filter target resolved by context target=%p context=%p",
                target,
                context_base);
        }
    }

    if (target == NULL || !memory_range_readable(target, sizeof(expected))) {
        append_logf("KBO all-star candidate player push filter hook target unreadable target=%p", target);
        return 0;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star candidate player push filter hook", target, sizeof(expected));
        log_extended_context("KBO all-star candidate player push filter hook", target, 16, 80);
        return 0;
    }

    void* vector_push_back_address = resolve_relative_call_target(
        target + OOTP27_ALLSTAR_CANDIDATE_PLAYER_PUSH_CALL_OFFSET);
    if (vector_push_back_address == NULL) {
        append_log_line("KBO all-star candidate player push filter hook skipped: vector push helper target unresolved");
        return 0;
    }

    void* return_address = target + sizeof(expected);
    void* skip_address = target - OOTP27_ALLSTAR_CANDIDATE_PLAYER_PUSH_SKIP_DELTA;
    uint8_t* stub = build_allstar_candidate_player_push_filter_stub(
        return_address,
        skip_address,
        vector_push_back_address);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO all-star candidate player push filter hook stub");
        return 0;
    }

    uint8_t patch[OOTP27_ALLSTAR_CANDIDATE_PLAYER_PUSH_STOLEN_LEN] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0,
        0x90
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO all-star candidate player push filter hook error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);
    append_logf(
        "installed KBO all-star candidate player push filter hook target=%p stub=%p return=%p skip=%p vector_push=%p helper=%p",
        target,
        stub,
        return_address,
        skip_address,
        vector_push_back_address,
        &ootp_kbo_allstar_candidate_push_filter);
    return 1;
}

