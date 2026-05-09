#include "hook_stubs_foreign_counts.h"
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

uint8_t* build_kbo_active_foreign_hitter_count_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x48, 0xBA,                                     // mov rdx, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_active_foreign_hitter_count_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_active_foreign_pitcher_count_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x48, 0xBA,                                     // mov rdx, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_active_foreign_pitcher_count_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_callup_foreign_limit_branch_stub(
    void* allow_continuation,
    void* block_continuation,
    void* fail_continuation,
    void* wrapper,
    int total_check)
{
    uint8_t code[112] = {
        0x45, 0x89, 0xC8,                               // mov r8d,r9d placeholder
        0x41, 0x89, 0xD1,                               // mov r9d,edx placeholder
        0x48, 0x89, 0xFA,                               // mov rdx,rdi
        0x49, 0x8B, 0xCF,                               // mov rcx,r15
        0x48, 0xB8,                                     // mov rax,wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x84, 0xC0,                                     // test al,al
        0x74, 0x0C,                                     // je not_allowed
        0x48, 0xB8,                                     // mov rax,allow
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x45, 0x84, 0xE4,                               // test r12b,r12b
        0x74, 0x0C,                                     // je block
        0x48, 0xB8,                                     // mov rax,fail
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0xB8,                                     // mov rax,block
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    if (total_check) {
        code[0] = 0x41; code[1] = 0x89; code[2] = 0xD0; /* mov r8d,edx */
        code[3] = 0x41; code[4] = 0x89; code[5] = 0xC9; /* mov r9d,ecx */
    } else {
        code[0] = 0x41; code[1] = 0x89; code[2] = 0xC0; /* mov r8d,eax */
        code[3] = 0x41; code[4] = 0x89; code[5] = 0xD1; /* mov r9d,edx */
    }

    write_u64(&code[14], (uint64_t)(uintptr_t)wrapper);
    write_u64(&code[30], (uint64_t)(uintptr_t)allow_continuation);
    write_u64(&code[47], (uint64_t)(uintptr_t)fail_continuation);
    write_u64(&code[59], (uint64_t)(uintptr_t)block_continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
