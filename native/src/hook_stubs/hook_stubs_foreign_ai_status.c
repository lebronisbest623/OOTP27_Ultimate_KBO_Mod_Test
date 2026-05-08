#include "hook_stubs_foreign_ai_status.h"
#include <stdio.h>
#include <string.h>
#include "../bootstrap/ootp_offsets.h"
#include "../core/core_log.h"
#include "../core/core_current_date.h"
#include "../core/core_save_paths.h"
#include "../core/core_text_date.h"
#include "../core/core_flags/flags_api.h"
#include "../runtime_memory/runtime_memory.h"
#include "../patch_helpers/patch_helpers.h"
#include "../bootstrap/hook_entrypoints.h"

uint8_t* build_kbo_ai_fa_status_candidate_insert_stub(void* continuation)
{
    uint8_t code[80] = {
        0x41, 0x89, 0xC8,                               // mov r8d, ecx
        0x48, 0x89, 0xE9,                               // mov rcx, rbp
        0x4C, 0x89, 0xFA,                               // mov rdx, r15
        0x4D, 0x89, 0xF1,                               // mov r9, r14
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x89, 0xC1,                                     // mov ecx, eax
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[14], (uint64_t)(uintptr_t)&ootp_kbo_ai_fa_status_candidate_insert_wrapper);
    write_u64(&code[28], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
