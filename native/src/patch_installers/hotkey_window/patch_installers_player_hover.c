#include "patch_installers_player_hover.h"
#include <string.h>
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../build_verify/build_verify.h"
#include "../../core/logging/core_log.h"
#include "../../hook_stubs/hotkey_window/hook_stubs_player_hover.h"
#include "../../hook_stubs/military/hook_stubs_military.h"
#include "../../hotkey_window/runtime/player_hover/player_hover_manager_probe.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"

int install_kbo_player_hover_manager_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO player hover manager probe patch");
        return 0;
    }

    const uint8_t expected[OOTP27_PLAYER_TOOLTIP_HOVER_MANAGER_STOLEN_LEN] = {
        0x40, 0x53,
        0x55,
        0x56,
        0x57,
        0x41, 0x54,
        0x41, 0x55,
        0x41, 0x56,
        0x41, 0x57,
        0x48, 0x83, 0xEC, 0x58
    };

    uint8_t* direct_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_PLAYER_TOOLTIP_HOVER_MANAGER_RVA);
    if (direct_target != NULL &&
        memory_range_readable(direct_target, 12u) &&
        is_rax_absolute_jump_patch(direct_target)) {
        kbo_log_runtimef("KBO player hover manager probe patch already installed target=%p", direct_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_PLAYER_TOOLTIP_HOVER_MANAGER_RVA,
        expected,
        sizeof(expected),
        "KBO player hover manager probe patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO player hover manager probe patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(
        target,
        OOTP27_PLAYER_TOOLTIP_HOVER_MANAGER_STOLEN_LEN);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player hover manager trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_player_hover_manager_probe_detour_stub(trampoline);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player hover manager probe stub");
        return 0;
    }

    uint8_t patch[OOTP27_PLAYER_TOOLTIP_HOVER_MANAGER_STOLEN_LEN] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO player hover manager probe patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO player hover manager probe patch target=%p trampoline=%p stub=%p",
        target,
        trampoline,
        stub);
    return 1;
}

int install_kbo_player_tooltip_text_append_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO player tooltip text append probe patch");
        return 0;
    }

    const uint8_t expected[OOTP27_PLAYER_TOOLTIP_TEXT_APPEND_STOLEN_LEN] = {
        0x44, 0x88, 0x4C, 0x24, 0x20,
        0x48, 0x89, 0x4C, 0x24, 0x08,
        0x53,
        0x55,
        0x56,
        0x57,
        0x41, 0x54,
        0x41, 0x57
    };

    uint8_t* direct_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_PLAYER_TOOLTIP_TEXT_APPEND_RVA);
    if (direct_target != NULL &&
        memory_range_readable(direct_target, 12u) &&
        is_rax_absolute_jump_patch(direct_target)) {
        kbo_log_runtimef("KBO player tooltip text append probe patch already installed target=%p", direct_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_PLAYER_TOOLTIP_TEXT_APPEND_RVA,
        expected,
        sizeof(expected),
        "KBO player tooltip text append probe patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO player tooltip text append probe patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(
        target,
        OOTP27_PLAYER_TOOLTIP_TEXT_APPEND_STOLEN_LEN);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player tooltip text append trampoline");
        return 0;
    }
    kbo_set_player_tooltip_text_append_original((uintptr_t)trampoline);

    uint8_t* stub = build_kbo_player_tooltip_text_append_probe_detour_stub();
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player tooltip text append probe stub");
        return 0;
    }

    uint8_t patch[OOTP27_PLAYER_TOOLTIP_TEXT_APPEND_STOLEN_LEN] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO player tooltip text append probe patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO player tooltip text append probe patch target=%p trampoline=%p stub=%p",
        target,
        trampoline,
        stub);
    return 1;
}

int install_kbo_player_tooltip_string_format_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO player tooltip string format probe patch");
        return 0;
    }

    const uint8_t expected[OOTP27_PLAYER_TOOLTIP_STRING_FORMAT_STOLEN_LEN] = {
        0x48, 0x85, 0xD2,
        0x0F, 0x84, 0x6E, 0x01, 0x00, 0x00,
        0x48, 0x8B, 0xC4,
        0x48, 0x89, 0x50, 0x10,
        0x4C, 0x89, 0x40, 0x18,
        0x4C, 0x89, 0x48, 0x20
    };

    uint8_t* direct_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_PLAYER_TOOLTIP_STRING_FORMAT_RVA);
    if (direct_target != NULL &&
        memory_range_readable(direct_target, 12u) &&
        is_rax_absolute_jump_patch(direct_target)) {
        kbo_log_runtimef("KBO player tooltip string format probe patch already installed target=%p", direct_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_PLAYER_TOOLTIP_STRING_FORMAT_RVA,
        expected,
        sizeof(expected),
        "KBO player tooltip string format probe patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO player tooltip string format probe patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(
        target,
        OOTP27_PLAYER_TOOLTIP_STRING_FORMAT_STOLEN_LEN);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player tooltip string format trampoline");
        return 0;
    }
    kbo_set_player_tooltip_string_format_original((uintptr_t)trampoline);

    uint8_t* stub = build_kbo_player_tooltip_string_format_probe_detour_stub();
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player tooltip string format probe stub");
        return 0;
    }

    uint8_t patch[OOTP27_PLAYER_TOOLTIP_STRING_FORMAT_STOLEN_LEN] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO player tooltip string format probe patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO player tooltip string format probe patch target=%p trampoline=%p stub=%p",
        target,
        trampoline,
        stub);
    return 1;
}

int install_kbo_player_tooltip_rating_common_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO player tooltip rating common probe patch");
        return 0;
    }

    const uint8_t expected[OOTP27_PLAYER_TOOLTIP_RATING_COMMON_STOLEN_LEN] = {
        0x48, 0x8B, 0xC4,
        0x48, 0x89, 0x58, 0x08,
        0x48, 0x89, 0x68, 0x10,
        0x48, 0x89, 0x70, 0x18
    };

    uint8_t* direct_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_PLAYER_TOOLTIP_RATING_COMMON_RVA);
    if (direct_target != NULL &&
        memory_range_readable(direct_target, 12u) &&
        is_rax_absolute_jump_patch(direct_target)) {
        kbo_log_runtimef("KBO player tooltip rating common probe patch already installed target=%p", direct_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_PLAYER_TOOLTIP_RATING_COMMON_RVA,
        expected,
        sizeof(expected),
        "KBO player tooltip rating common probe patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO player tooltip rating common probe patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(
        target,
        OOTP27_PLAYER_TOOLTIP_RATING_COMMON_STOLEN_LEN);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player tooltip rating common trampoline");
        return 0;
    }
    kbo_set_player_tooltip_rating_common_original((uintptr_t)trampoline);

    uint8_t* stub = build_kbo_player_tooltip_rating_common_probe_detour_stub();
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player tooltip rating common probe stub");
        return 0;
    }

    uint8_t patch[OOTP27_PLAYER_TOOLTIP_RATING_COMMON_STOLEN_LEN] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO player tooltip rating common probe patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO player tooltip rating common probe patch target=%p trampoline=%p stub=%p",
        target,
        trampoline,
        stub);
    return 1;
}

