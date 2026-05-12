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
#include "../../team/add_player_guard/team_add_player_guard_ai_roster.h"
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
