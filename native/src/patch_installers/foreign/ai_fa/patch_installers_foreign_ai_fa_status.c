#include "patch_installers_foreign_ai_fa_status.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../hook_stubs/foreign/ai_status/hook_stubs_foreign_ai_status.h"

int install_kbo_ai_fa_status_candidate_insert_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("GetModuleHandleA(NULL) failed for KBO AI FA status candidate insert patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (strstr(host, "ootp27.exe") == NULL && strstr(host, "OOTP27.EXE") == NULL) {
        append_logf("host is not ootp27.exe, skipping KBO AI FA status candidate insert patch host=%s", host);
        return 0;
    }

    const uint8_t expected[12] = {
        0x48, 0x63, 0xC1,                               /* movsxd rax,ecx */
        0x4D, 0x89, 0x3C, 0xC6,                         /* mov [r14+rax*8],r15 */
        0xFF, 0xC1,                                     /* inc ecx */
        0x89, 0x4D, 0xD4                                /* mov [rbp-0x2c],ecx */
    };
    const uint8_t context[48] = {
        0x8B, 0x9D, 0x58, 0x01, 0x00, 0x00, 0x4C, 0x8B,
        0xA5, 0x50, 0x01, 0x00, 0x00, 0x8B, 0x4D, 0xD4,
        0x48, 0x63, 0xC1, 0x4D, 0x89, 0x3C, 0xC6, 0xFF,
        0xC1, 0x89, 0x4D, 0xD4, 0xEB, 0x19, 0x4C, 0x8B,
        0x75, 0xC8, 0x48, 0x63, 0xC1, 0x4D, 0x89, 0x3C,
        0xC6, 0xFF, 0xC1, 0x89, 0x4D, 0xD4, 0xEB, 0x07
    };

    uint8_t* target = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_AI_FA_STATUS_CANDIDATE_INSERT_RVA,
        expected,
        sizeof(expected),
        context,
        sizeof(context),
        16u,
        "KBO AI FA status candidate insert patch");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        append_logf("KBO AI FA status candidate insert patch already installed target=%p", target);
        return 1;
    }

    uint8_t* stub = build_kbo_ai_fa_status_candidate_insert_stub(target + sizeof(expected));
    if (stub == NULL) {
        append_log_line("failed to allocate KBO AI FA status candidate insert stub");
        return 0;
    }

    uint8_t patch[12] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      /* jmp rax */
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for KBO AI FA status candidate insert patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    append_logf(
        "installed KBO AI FA status candidate insert patch target=%p stub=%p wrapper=%p",
        target,
        stub,
        &ootp_kbo_ai_fa_status_candidate_insert_wrapper);
    return 1;
}
