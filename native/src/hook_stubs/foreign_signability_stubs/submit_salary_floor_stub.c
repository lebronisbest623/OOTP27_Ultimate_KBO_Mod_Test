#include "submit_salary_floor_stub.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_log.h"
#include "../../core/core_current_date.h"
#include "../../core/core_save_paths.h"
#include "../../core/core_text_date.h"
#include "../../core/core_flags/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../patch_helpers/patch_helpers.h"

uint8_t* build_kbo_no_minor_contract_submit_salary_floor_stub(
    void* zero_salary_continuation,
    void* min_check_continuation,
    void* after_min_check_continuation)
{
    uint8_t code[96] = {
        0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x01,       // mov byte ptr [rdi+0xcc], 1
        0x8B, 0x87, 0xC0, 0x00, 0x00, 0x00,             // mov eax, [rdi+0xc0]
        0x4D, 0x85, 0xF6,                               // test r14, r14
        0x74, 0x21,                                     // je fallback_to_original
        0x41, 0x8B, 0x8E, 0x78, 0x02, 0x00, 0x00,       // mov ecx, [r14+0x278]
        0x85, 0xC9,                                     // test ecx, ecx
        0x7E, 0x16,                                     // jle fallback_to_original
        0x3B, 0xC1,                                     // cmp eax, ecx
        0x7D, 0x06,                                     // jge continue_after_min_check
        0x89, 0x8F, 0xC0, 0x00, 0x00, 0x00,             // mov [rdi+0xc0], ecx
        0x48, 0xB8,                                     // mov rax, after_min_check_continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x85, 0xC0,                                     // test eax, eax
        0x75, 0x0C,                                     // jne continue_min_check
        0x48, 0xB8,                                     // mov rax, zero_salary_continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0xB8,                                     // mov rax, min_check_continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC
    };

    write_u64(&code[41], (uint64_t)(uintptr_t)after_min_check_continuation);
    write_u64(&code[57], (uint64_t)(uintptr_t)zero_salary_continuation);
    write_u64(&code[69], (uint64_t)(uintptr_t)min_check_continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
