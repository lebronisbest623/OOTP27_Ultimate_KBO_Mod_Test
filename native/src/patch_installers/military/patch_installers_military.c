#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../build_verify/build_verify.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../hook_stubs/military/hook_stubs_military.h"
#include "../../team/add_player_guard/team_add_player_guard.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"
#include "patch_installers_military.h"
int install_kbo_military_service_entry_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO military service entry patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO military service entry patch host=%s", host);
        return 0;
    }

    const uint8_t expected_signature[] = {
        0x40, 0x55,                                     // push rbp
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x41, 0x56,                                     // push r14
        0x41, 0x57,                                     // push r15
        0x48, 0x8D, 0x6C, 0x24, 0xE1                    // lea rbp, [rsp - 0x1f]
    };
    uint8_t* target = find_ootp_executable_pattern(expected_signature, sizeof(expected_signature));
    if (target == NULL) {
        append_log_line("Could not resolve military service entry function by signature");
        return 0;
    }
    const size_t stolen_len = 18;
    uint8_t expected[18] = {
        0x40, 0x55,                                     // push rbp
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x41, 0x56,                                     // push r14
        0x41, 0x57,                                     // push r15
        0x48, 0x8D, 0x6C, 0x24, 0xE1                    // lea rbp, [rsp - 0x1f]
    };

    if (target[0] == 0x48 && target[1] == 0xB8 && target[10] == 0xFF && target[11] == 0xE0) {
        append_logf("KBO military service entry patch already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO military service entry patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO military service entry trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_military_service_entry_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO military service entry detour stub");
        return 0;
    }

    uint8_t patch[18] = {
        0x48, 0xB8,                                     // mov rax, stub
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO military service entry patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO military service entry patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_military_service_entry_wrapper);
    return 1;
}

int install_kbo_military_status_update_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO military status update patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO military status update patch host=%s", host);
        return 0;
    }

    const uint8_t expected_signature[] = {
        0x48, 0x89, 0x4C, 0x24, 0x08,                   // mov [rsp + 8], rcx
        0x55,                                           // push rbp
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x41, 0x56,                                     // push r14
        0x41, 0x57                                      // push r15
    };
    uint8_t* target = find_ootp_executable_pattern(expected_signature, sizeof(expected_signature));
    if (target == NULL) {
        append_log_line("Could not resolve military status update function by signature");
        return 0;
    }
    const size_t stolen_len = 17;
    uint8_t expected[17] = {
        0x48, 0x89, 0x4C, 0x24, 0x08,                   // mov [rsp + 8], rcx
        0x55,                                           // push rbp
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x57,                                           // push rdi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x41, 0x56,                                     // push r14
        0x41, 0x57                                      // push r15
    };

    if (target[0] == 0x48 && target[1] == 0xB8 && target[10] == 0xFF && target[11] == 0xE0) {
        append_logf("KBO military status update patch already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO military status update patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO military status update trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_military_status_update_detour_stub(trampoline);
    if (stub == NULL) {
        append_log_line("failed to allocate KBO military status update detour stub");
        return 0;
    }

    uint8_t patch[17] = {
        0x48, 0xB8,                                     // mov rax, stub
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO military status update patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO military status update patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_military_status_update_wrapper);
    return 1;
}

int install_kbo_military_team_add_guard_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO military team-add guard patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO military team-add guard patch host=%s", host);
        return 0;
    }

    const size_t stolen_len = 16;
    const uint8_t expected[16] = {
        0x44, 0x89, 0x4C, 0x24, 0x20,                  /* mov [rsp+0x20], r9d */
        0x4C, 0x89, 0x44, 0x24, 0x18,                  /* mov [rsp+0x18], r8 */
        0x48, 0x89, 0x4C, 0x24, 0x08,                  /* mov [rsp+0x8], rcx */
        0x55                                            /* push rbp */
    };
    uint8_t* rva_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_TEAM_ADD_PLAYER_RVA);
    if (memory_range_readable(rva_target, sizeof(expected)) && is_rax_absolute_jump_patch(rva_target)) {
        append_logf("KBO military team-add guard patch already installed target=%p", rva_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_TEAM_ADD_PLAYER_RVA,
        expected,
        sizeof(expected),
        "KBO military team-add guard patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO military team-add guard patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO military team-add guard trampoline");
        return 0;
    }
    kbo_set_team_add_player_guard_trampoline(trampoline);

    uint8_t* stub = build_kbo_team_add_player_guard_detour_stub();
    if (stub == NULL) {
        append_log_line("failed to allocate KBO military team-add guard detour stub");
        kbo_clear_team_add_player_guard_trampoline();
        return 0;
    }

    uint8_t patch[16] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO military team-add guard patch error=%lu", GetLastError());
        kbo_clear_team_add_player_guard_trampoline();
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO military team-add guard patch target=%p rva=0x%llx stub=%p trampoline=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        trampoline,
        &ootp_kbo_team_add_player_guard_wrapper);
    return 1;
}

typedef void (*KboRosterMoveTraceSetTrampolineFn)(void*);

static int install_kbo_foreign_roster_move_trace_patch(
    HMODULE exe,
    const char* label,
    uint32_t target_rva,
    const uint8_t* expected,
    size_t patch_len,
    void* wrapper,
    KboRosterMoveTraceSetTrampolineFn set_trampoline)
{
    uint8_t* rva_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, target_rva);
    if (memory_range_readable(rva_target, patch_len) && is_rax_absolute_jump_patch(rva_target)) {
        append_logf("%s already installed target=%p", label, rva_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        target_rva,
        expected,
        patch_len,
        label);
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, patch_len);
    if (trampoline == NULL) {
        append_logf("failed to allocate %s trampoline", label);
        return 0;
    }
    set_trampoline(trampoline);

    uint8_t patch[32] = {0};
    if (patch_len > sizeof(patch) || patch_len < 12) {
        append_logf("%s invalid patch_len=%llu", label, (unsigned long long)patch_len);
        return 0;
    }
    memset(patch, 0x90, patch_len);
    patch[0] = 0x48;
    patch[1] = 0xB8;
    write_u64(&patch[2], (uint64_t)(uintptr_t)wrapper);
    patch[10] = 0xFF;
    patch[11] = 0xE0;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, patch_len);
    FlushInstructionCache(GetCurrentProcess(), target, patch_len);

    DWORD ignored = 0;
    VirtualProtect(target, patch_len, old_protect, &ignored);

    append_logf(
        "installed %s target=%p rva=0x%llx stub=%p trampoline=%p wrapper=%p",
        label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        wrapper,
        trampoline,
        wrapper);
    return 1;
}

static uint8_t* build_kbo_ai_roster_post_sort_gate_score_trampoline(uint8_t* target)
{
    if (target == NULL) {
        return NULL;
    }

    uint8_t* memory = (uint8_t*)VirtualAlloc(
        NULL,
        64u,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }

    const uintptr_t fail_target = (uintptr_t)target + 0x3cfu;
    const uintptr_t continue_target = (uintptr_t)target + 17u;
    size_t n = 0;

    memory[n++] = 0x48;
    memory[n++] = 0x83;
    memory[n++] = 0xEC;
    memory[n++] = 0x28;
    memory[n++] = 0x83;
    memory[n++] = 0xB9;
    memory[n++] = 0x5C;
    memory[n++] = 0x14;
    memory[n++] = 0x00;
    memory[n++] = 0x00;
    memory[n++] = 0x00;
    memory[n++] = 0x0F;
    memory[n++] = 0x85;
    memory[n++] = 0x0C;
    memory[n++] = 0x00;
    memory[n++] = 0x00;
    memory[n++] = 0x00;
    memory[n++] = 0x48;
    memory[n++] = 0xB8;
    write_u64(&memory[n], (uint64_t)fail_target);
    n += 8;
    memory[n++] = 0xFF;
    memory[n++] = 0xE0;
    memory[n++] = 0x48;
    memory[n++] = 0xB8;
    write_u64(&memory[n], (uint64_t)continue_target);
    n += 8;
    memory[n++] = 0xFF;
    memory[n++] = 0xE0;

    FlushInstructionCache(GetCurrentProcess(), memory, n);
    return memory;
}

int install_kbo_foreign_roster_move_trace_patches(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO foreign roster-move trace patches");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO foreign roster-move trace patches host=%s", host);
        return 0;
    }

    const uint8_t active_expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x20,
        0x44, 0x88, 0x44, 0x24, 0x18,
        0x48, 0x89, 0x54, 0x24, 0x10,
        0x48, 0x89, 0x4C, 0x24, 0x08
    };
    const uint8_t secondary_expected[15] = {
        0x44, 0x88, 0x44, 0x24, 0x18,
        0x48, 0x89, 0x54, 0x24, 0x10,
        0x48, 0x89, 0x4C, 0x24, 0x08
    };
    const uint8_t assignment_expected[15] = {
        0x44, 0x88, 0x4C, 0x24, 0x20,
        0x44, 0x88, 0x44, 0x24, 0x18,
        0x53,
        0x56,
        0x57,
        0x41, 0x55
    };

    int ok = 1;
    ok &= install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO foreign roster-move active trace A52950",
        OOTP27_ROSTER_MOVE_ACTIVE_TRACE_RVA,
        active_expected,
        sizeof(active_expected),
        &ootp_kbo_roster_move_active_trace_wrapper,
        &kbo_set_roster_move_active_trace_trampoline);
    ok &= install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO foreign roster-move secondary trace A565F0",
        OOTP27_ROSTER_MOVE_SECONDARY_TRACE_RVA,
        secondary_expected,
        sizeof(secondary_expected),
        &ootp_kbo_roster_move_secondary_trace_wrapper,
        &kbo_set_roster_move_secondary_trace_trampoline);
    ok &= install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO foreign roster-move assignment trace AB9280",
        OOTP27_ROSTER_MOVE_ASSIGNMENT_TRACE_RVA,
        assignment_expected,
        sizeof(assignment_expected),
        &ootp_kbo_roster_move_assignment_trace_wrapper,
        &kbo_set_roster_move_assignment_trace_trampoline);
    return ok;
}

int install_kbo_player_team_assignment_trace_patches(void)
{
    if (read_kbo_localappdata_flag_file("disable_kbo_player_team_assignment_trace.txt")) {
        append_log_line("KBO player team assignment trace skipped: disable_kbo_player_team_assignment_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO player team assignment trace patches");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO player team assignment trace patches host=%s", host);
        return 0;
    }

    const uint8_t clear_expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x57,
        0x48, 0x83, 0xEC, 0x20,
        0x83, 0x79, 0x58, 0x00,
        0x0F, 0xB6, 0xFA,
        0x48, 0x8B, 0xD9
    };
    const uint8_t set_expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x6C, 0x24, 0x10,
        0x48, 0x89, 0x74, 0x24, 0x18,
        0x57,
        0x48, 0x83, 0xEC, 0x20
    };

    int ok = 1;
    ok &= install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO player clear-team trace 7D7870",
        OOTP27_PLAYER_CLEAR_TEAM_TRACE_RVA,
        clear_expected,
        sizeof(clear_expected),
        &ootp_kbo_player_clear_team_trace_wrapper,
        &kbo_set_player_clear_team_trace_trampoline);
    ok &= install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO player set-team trace 7D78E0",
        OOTP27_PLAYER_SET_TEAM_TRACE_RVA,
        set_expected,
        sizeof(set_expected),
        &ootp_kbo_player_set_team_trace_wrapper,
        &kbo_set_player_set_team_trace_trampoline);
    return ok;
}

int install_kbo_player_eval_double_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_foreign_roster_eval_bias.txt")) {
        append_log_line("KBO player eval double trace skipped: disable_foreign_roster_eval_bias is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO player eval double trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO player eval double trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[18] = {
        0x44, 0x88, 0x4C, 0x24, 0x20,
        0x88, 0x54, 0x24, 0x10,
        0x55,
        0x57,
        0x48, 0x81, 0xEC, 0xE8, 0x00, 0x00, 0x00
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO player eval double trace 8714F0",
        OOTP27_PLAYER_EVAL_DOUBLE_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_player_eval_double_trace_wrapper,
        &kbo_set_player_eval_double_trace_trampoline);
}

int install_kbo_player_eval_cache_trace_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO player eval cache trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO player eval cache trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[18] = {
        0x48, 0x8B, 0xC4,
        0x66, 0x44, 0x89, 0x48, 0x20,
        0x89, 0x50, 0x10,
        0x55,
        0x53,
        0x56,
        0x57,
        0x41, 0x54,
        0x41
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO player eval cache trace 811450",
        OOTP27_PLAYER_EVAL_CACHE_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_player_eval_cache_trace_wrapper,
        &kbo_set_player_eval_cache_trace_trampoline);
}

int install_kbo_ai_player_quality_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_player_quality_trace.txt")) {
        append_log_line("KBO AI player-quality trace skipped: disable_ai_player_quality_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI player-quality trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI player-quality trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[18] = {
        0x48, 0x8B, 0xC4,
        0x48, 0x89, 0x58, 0x20,
        0x44, 0x88, 0x40, 0x18,
        0x48, 0x89, 0x48, 0x08,
        0x55,
        0x56,
        0x57
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI player-quality trace 833900",
        OOTP27_AI_PLAYER_QUALITY_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_player_quality_trace_wrapper,
        &kbo_set_ai_player_quality_trace_trampoline);
}

int install_kbo_ai_roster_role_check_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_role_check_trace.txt")) {
        append_log_line("KBO AI roster role-check trace skipped: disable_ai_roster_role_check_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster role-check trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster role-check trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[12] = {
        0x41, 0xB8, 0x05, 0x00, 0x00, 0x00,
        0x45, 0x33, 0xC9,
        0x41, 0x8B, 0xC0
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster role-check trace 8352B0",
        OOTP27_AI_ROSTER_ROLE_CHECK_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_role_check_trace_wrapper,
        &kbo_set_ai_roster_role_check_trace_trampoline);
}

int install_kbo_ai_roster_post_sort_gate_score_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_post_sort_gate_score_trace.txt")) {
        append_log_line("KBO AI roster post-sort gate score trace skipped: disable_ai_roster_post_sort_gate_score_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster post-sort gate score trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster post-sort gate score trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[17] = {
        0x48, 0x83, 0xEC, 0x28,
        0x83, 0xB9, 0x5C, 0x14, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0xBE, 0x03, 0x00, 0x00
    };

    uint8_t* rva_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_AI_ROSTER_POST_SORT_GATE_SCORE_TRACE_RVA);
    if (memory_range_readable(rva_target, sizeof(expected)) && is_rax_absolute_jump_patch(rva_target)) {
        append_logf("KBO AI roster post-sort gate score trace 834ED0 already installed target=%p", rva_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_AI_ROSTER_POST_SORT_GATE_SCORE_TRACE_RVA,
        expected,
        sizeof(expected),
        "KBO AI roster post-sort gate score trace 834ED0");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO AI roster post-sort gate score trace 834ED0 already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO AI roster post-sort gate score trace 834ED0", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_ai_roster_post_sort_gate_score_trampoline(target);
    if (trampoline == NULL) {
        append_log_line("failed to allocate KBO AI roster post-sort gate score trace trampoline");
        return 0;
    }
    kbo_set_ai_roster_post_sort_gate_score_trace_trampoline(trampoline);

    uint8_t patch[17] = {0};
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0x48;
    patch[1] = 0xB8;
    write_u64(&patch[2], (uint64_t)(uintptr_t)&ootp_kbo_ai_roster_post_sort_gate_score_trace_wrapper);
    patch[10] = 0xFF;
    patch[11] = 0xE0;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO AI roster post-sort gate score trace 834ED0 error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO AI roster post-sort gate score trace 834ED0 target=%p rva=0x%llx trampoline=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        trampoline,
        &ootp_kbo_ai_roster_post_sort_gate_score_trace_wrapper);
    return 1;
}

int install_kbo_ai_team_player_fit_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_team_player_fit_trace.txt")) {
        append_log_line("KBO AI team-player fit trace skipped: disable_ai_team_player_fit_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI team-player fit trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI team-player fit trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[18] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x74, 0x24, 0x10,
        0x48, 0x89, 0x7C, 0x24, 0x20,
        0x41, 0x56,
        0x48
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI team-player fit trace A35560",
        OOTP27_AI_TEAM_PLAYER_FIT_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_team_player_fit_trace_wrapper,
        &kbo_set_ai_team_player_fit_trace_trampoline);
}

int install_kbo_ai_roster_eligibility_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_eligibility_trace.txt")) {
        append_log_line("KBO AI roster eligibility trace skipped: disable_ai_roster_eligibility_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster eligibility trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster eligibility trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x6C, 0x24, 0x10,
        0x48, 0x89, 0x74, 0x24, 0x18,
        0x48, 0x89, 0x7C, 0x24, 0x20
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster eligibility trace 836800",
        OOTP27_AI_ROSTER_ELIGIBILITY_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_eligibility_trace_wrapper,
        &kbo_set_ai_roster_eligibility_trace_trampoline);
}

int install_kbo_ai_roster_availability_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_availability_trace.txt")) {
        append_log_line("KBO AI roster availability trace skipped: disable_ai_roster_availability_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster availability trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster availability trace patch host=%s", host);
        return 0;
    }

    void* by_id_lookup = kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TEAM_STATUS_BY_ID_LOOKUP_RVA);
    if (by_id_lookup != NULL) {
        kbo_set_player_team_status_by_id_lookup_fn(by_id_lookup);
    } else {
        append_log_line("KBO AI roster availability trace warning: by-id status lookup is null");
    }

    const uint8_t expected[13] = {
        0x40,
        0x53,
        0x48, 0x83, 0xEC, 0x20,
        0x80, 0xB9, 0x78, 0x0C, 0x00, 0x00, 0x01
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster availability trace 836BF0",
        OOTP27_AI_ROSTER_AVAILABILITY_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_availability_trace_wrapper,
        &kbo_set_ai_roster_availability_trace_trampoline);
}

int install_kbo_ai_roster_f65_update_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_f65_update_trace.txt")) {
        append_log_line("KBO AI roster f65 update trace skipped: disable_ai_roster_f65_update_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster f65 update trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster f65 update trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[13] = {
        0x53,
        0x56,
        0x57,
        0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00,
        0x49, 0x63, 0xF0
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster f65 update trace D98630",
        OOTP27_AI_ROSTER_F65_UPDATE_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_f65_update_trace_wrapper,
        &kbo_set_ai_roster_f65_update_trace_trampoline);
}

int install_kbo_player_team_status_lookup_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_player_team_status_lookup_trace.txt")) {
        append_log_line("KBO player-team status lookup trace skipped: disable_player_team_status_lookup_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO player-team status lookup trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO player-team status lookup trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[17] = {
        0x48, 0x89, 0x5C, 0x24, 0x10,
        0x48, 0x89, 0x74, 0x24, 0x18,
        0x48, 0x89, 0x7C, 0x24, 0x20,
        0x41, 0x56
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO player-team status lookup trace 7EF490",
        OOTP27_PLAYER_TEAM_STATUS_LOOKUP_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_player_team_status_lookup_trace_wrapper,
        &kbo_set_player_team_status_lookup_trace_trampoline);
}

int install_kbo_player_default_status_lookup_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_player_default_status_lookup_trace.txt")) {
        append_log_line("KBO player-default status lookup trace skipped: disable_player_default_status_lookup_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO player-default status lookup trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO player-default status lookup trace patch host=%s", host);
        return 0;
    }

    void* by_id_lookup = kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TEAM_STATUS_BY_ID_LOOKUP_RVA);
    if (by_id_lookup == NULL) {
        append_log_line("KBO player-default status lookup trace patch failed: by-id status lookup is null");
        return 0;
    }
    kbo_set_player_team_status_by_id_lookup_fn(by_id_lookup);

    const uint8_t expected[12] = {
        0x48, 0x8B, 0x81, 0x88, 0x0E, 0x00, 0x00,
        0x48, 0x85, 0xC0,
        0x75, 0x0B
    };

    uint8_t* rva_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_PLAYER_DEFAULT_STATUS_LOOKUP_TRACE_RVA);
    if (memory_range_readable(rva_target, sizeof(expected)) && is_rax_absolute_jump_patch(rva_target)) {
        append_logf("KBO player-default status lookup trace 7EF820 already installed target=%p", rva_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_PLAYER_DEFAULT_STATUS_LOOKUP_TRACE_RVA,
        expected,
        sizeof(expected),
        "KBO player-default status lookup trace 7EF820");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO player-default status lookup trace 7EF820 already installed target=%p", target);
        return 1;
    }

    uint8_t patch[12] = {0};
    patch[0] = 0x48;
    patch[1] = 0xB8;
    write_u64(&patch[2], (uint64_t)(uintptr_t)&ootp_kbo_player_default_status_lookup_trace_wrapper);
    patch[10] = 0xFF;
    patch[11] = 0xE0;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO player-default status lookup trace 7EF820 error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO player-default status lookup trace 7EF820 target=%p rva=0x%llx wrapper=%p by_id=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        &ootp_kbo_player_default_status_lookup_trace_wrapper,
        by_id_lookup);
    return 1;
}

int install_kbo_pointer_vector_push_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_pointer_vector_push_trace.txt")) {
        append_log_line("KBO pointer-vector push trace skipped: disable_pointer_vector_push_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO pointer-vector push trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO pointer-vector push trace patch host=%s", host);
        return 0;
    }

    const uint8_t thunk_expected[12] = {
        0xE9, 0x5B, 0xD0, 0x87, 0x01,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC
    };
    uint8_t* thunk = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_POINTER_VECTOR_PUSH_TRACE_RVA);
    if (memory_range_readable(thunk, sizeof(thunk_expected))
            && memcmp(thunk, thunk_expected, sizeof(thunk_expected)) == 0) {
        int32_t rel = 0;
        memcpy(&rel, thunk + 1, sizeof(rel));
        uint8_t* resolved = thunk + 5 + rel;
        append_logf(
            "KBO pointer-vector push trace thunk 43D9E0 resolves to target=%p rva=0x%llx",
            resolved,
            (unsigned long long)((uintptr_t)resolved - (uintptr_t)exe));
    } else if (memory_range_readable(thunk, sizeof(thunk_expected)) && is_rax_absolute_jump_patch(thunk)) {
        append_logf("KBO pointer-vector push trace thunk 43D9E0 already patched target=%p", thunk);
        return 1;
    } else {
        log_patch_bytes_mismatch("KBO pointer-vector push trace thunk 43D9E0", thunk, sizeof(thunk_expected));
    }

    const uint8_t expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x10,
        0x48, 0x89, 0x6C, 0x24, 0x18,
        0x48, 0x89, 0x74, 0x24, 0x20,
        0x57,
        0x48, 0x83, 0xEC, 0x20
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO pointer-vector push trace 1CBAA40",
        OOTP27_POINTER_VECTOR_PUSH_BACK_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_pointer_vector_push_trace_wrapper,
        &kbo_set_pointer_vector_push_trace_trampoline);
}

int install_kbo_pointer_vector_sort_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_pointer_vector_sort_trace.txt")) {
        append_log_line("KBO pointer-vector sort trace skipped: disable_pointer_vector_sort_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO pointer-vector sort trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO pointer-vector sort trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[14] = {
        0x48, 0x83, 0xEC, 0x48,
        0x48, 0x89, 0x54, 0x24, 0x30,
        0x48, 0x8D, 0x44, 0x24, 0x30
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO pointer-vector sort trace 1CBAB40",
        OOTP27_POINTER_VECTOR_SORT_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_pointer_vector_sort_trace_wrapper,
        &kbo_set_pointer_vector_sort_trace_trampoline);
}

int install_kbo_ai_roster_priority_compare_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_foreign_sort_bias.txt")) {
        append_log_line("KBO AI roster priority compare bias skipped: disable_ai_roster_foreign_sort_bias is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster priority compare patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster priority compare patch host=%s", host);
        return 0;
    }

    const uint8_t expected[15] = {
        0x0F, 0xB7, 0x81, 0x06, 0x0F, 0x00, 0x00,
        0x44, 0x0F, 0xB7, 0x8A, 0x06, 0x0F, 0x00, 0x00
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster priority compare foreign f06 bias 917380",
        OOTP27_AI_ROSTER_PRIORITY_COMPARE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_priority_compare_wrapper,
        &kbo_set_ai_roster_priority_compare_trampoline);
}

int install_kbo_ai_roster_type_compare_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_foreign_sort_bias.txt")) {
        append_log_line("KBO AI roster type compare bias skipped: disable_ai_roster_foreign_sort_bias is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster type compare patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster type compare patch host=%s", host);
        return 0;
    }

    const uint8_t expected[15] = {
        0x0F, 0xB7, 0x81, 0x98, 0x0C, 0x00, 0x00,
        0x44, 0x0F, 0xB7, 0x8A, 0x98, 0x0C, 0x00, 0x00
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster type compare foreign c98 bias 909FA0",
        OOTP27_AI_ROSTER_TYPE_COMPARE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_type_compare_wrapper,
        &kbo_set_ai_roster_type_compare_trampoline);
}

int install_kbo_ai_roster_score_compare_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_foreign_sort_bias.txt")) {
        append_log_line("KBO AI roster score compare bias skipped: disable_ai_roster_foreign_sort_bias is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster score compare patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster score compare patch host=%s", host);
        return 0;
    }

    const uint8_t expected[16] = {
        0x8B, 0x81, 0xE0, 0x0F, 0x00, 0x00,
        0x44, 0x8B, 0x8A, 0xE0, 0x0F, 0x00, 0x00,
        0x4D, 0x85, 0xC0
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster score compare foreign fe bias 91E920",
        OOTP27_AI_ROSTER_SCORE_COMPARE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_score_compare_wrapper,
        &kbo_set_ai_roster_score_compare_trampoline);
}

int install_kbo_ai_roster_select_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_select_trace.txt")) {
        append_log_line("KBO AI roster select trace skipped: disable_ai_roster_select_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster select trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster select trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[12] = {
        0x48, 0x8B, 0xC4,
        0x44, 0x89, 0x40, 0x18,
        0x89, 0x50, 0x10,
        0x55,
        0x53
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster select trace DD86C0",
        OOTP27_AI_ROSTER_SELECT_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_select_trace_wrapper,
        &kbo_set_ai_roster_select_trace_trampoline);
}

int install_kbo_ai_roster_primary_apply_flow_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_primary_apply_flow_trace.txt")) {
        append_log_line("KBO AI roster primary apply-flow trace skipped: disable_ai_roster_primary_apply_flow_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster primary apply-flow trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster primary apply-flow trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[14] = {
        0x40, 0x53,
        0x48, 0x83, 0xEC, 0x30,
        0x48, 0x8B, 0xD9,
        0x48, 0x89, 0x6C, 0x24, 0x48
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster primary apply-flow trace DD1B20",
        OOTP27_AI_ROSTER_PRIMARY_APPLY_FLOW_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_primary_apply_flow_trace_wrapper,
        &kbo_set_ai_roster_primary_apply_flow_trace_trampoline);
}

int install_kbo_ai_roster_secondary_main_flow_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_secondary_main_flow_trace.txt")) {
        append_log_line("KBO AI roster secondary main-flow trace skipped: disable_ai_roster_secondary_main_flow_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster secondary main-flow trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster secondary main-flow trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[14] = {
        0x40, 0x57,
        0x41, 0x57,
        0x48, 0x81, 0xEC, 0x98, 0x00, 0x00, 0x00,
        0x45, 0x33, 0xFF
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster secondary main-flow trace DCE3B0",
        OOTP27_AI_ROSTER_SECONDARY_MAIN_FLOW_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_secondary_main_flow_trace_wrapper,
        &kbo_set_ai_roster_secondary_main_flow_trace_trampoline);
}

int install_kbo_ai_roster_secondary_alt_flow_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_secondary_alt_flow_trace.txt")) {
        append_log_line("KBO AI roster secondary alt-flow trace skipped: disable_ai_roster_secondary_alt_flow_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster secondary alt-flow trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster secondary alt-flow trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[16] = {
        0x48, 0x8B, 0xC4,
        0x57,
        0x48, 0x83, 0xEC, 0x70,
        0x48, 0x83, 0xB9, 0x30, 0x05, 0x00, 0x00, 0x00
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster secondary alt-flow trace DCDDC0",
        OOTP27_AI_ROSTER_SECONDARY_ALT_FLOW_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_secondary_alt_flow_trace_wrapper,
        &kbo_set_ai_roster_secondary_alt_flow_trace_trampoline);
}

int install_kbo_ai_roster_mark_selected_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_mark_selected_trace.txt")) {
        append_log_line("KBO AI roster mark-selected trace skipped: disable_ai_roster_mark_selected_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster mark-selected trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster mark-selected trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[15] = {
        0x48, 0x89, 0x5C, 0x24, 0x10,
        0x48, 0x89, 0x6C, 0x24, 0x18,
        0x48, 0x89, 0x74, 0x24, 0x20
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster mark-selected trace DCBE10",
        OOTP27_AI_ROSTER_MARK_SELECTED_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_mark_selected_trace_wrapper,
        &kbo_set_ai_roster_mark_selected_trace_trampoline);
}

int install_kbo_ai_roster_selection_reconcile_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_selection_reconcile_trace.txt")) {
        append_log_line("KBO AI roster selection reconcile trace skipped: disable_ai_roster_selection_reconcile_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster selection reconcile trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster selection reconcile trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[20] = {
        0x48, 0x8B, 0xC4,
        0x4C, 0x89, 0x40, 0x18,
        0x89, 0x50, 0x10,
        0x48, 0x89, 0x48, 0x08,
        0x55,
        0x53,
        0x56,
        0x57,
        0x41, 0x54
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster selection reconcile trace DE4160",
        OOTP27_AI_ROSTER_SELECTION_RECONCILE_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_selection_reconcile_trace_wrapper,
        &kbo_set_ai_roster_selection_reconcile_trace_trampoline);
}

int install_kbo_ai_roster_apply_selection_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file("disable_ai_roster_apply_selection_trace.txt")) {
        append_log_line("KBO AI roster apply-selection trace skipped: disable_ai_roster_apply_selection_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI roster apply-selection trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI roster apply-selection trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[17] = {
        0x44, 0x89, 0x4C, 0x24, 0x20,
        0x89, 0x54, 0x24, 0x10,
        0x53,
        0x56,
        0x41, 0x55,
        0x48, 0x83, 0xEC, 0x40
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster apply-selection trace DE6730",
        OOTP27_AI_ROSTER_APPLY_SELECTION_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_apply_selection_trace_wrapper,
        &kbo_set_ai_roster_apply_selection_trace_trampoline);
}
