#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "hook_stubs_cbt_draft_order.h"
#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../patch_helpers/patch_helpers.h"

uint8_t* build_kbo_cbt_draft_order_trampoline(void* original_address, size_t stolen_len)
{
    if (original_address == NULL || stolen_len == 0u || stolen_len > 64u) {
        return NULL;
    }

    size_t code_len = stolen_len + 16u;
    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, code_len, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }

    memcpy(memory, original_address, stolen_len);
    memory[stolen_len + 0] = 0x50;          /* push rax */
    memory[stolen_len + 1] = 0x48;          /* mov rax, original + stolen_len */
    memory[stolen_len + 2] = 0xB8;
    write_u64(&memory[stolen_len + 3], (uint64_t)((uintptr_t)original_address + stolen_len));
    memory[stolen_len + 11] = 0x48;         /* xchg qword ptr [rsp], rax */
    memory[stolen_len + 12] = 0x87;
    memory[stolen_len + 13] = 0x04;
    memory[stolen_len + 14] = 0x24;
    memory[stolen_len + 15] = 0xC3;         /* ret */
    FlushInstructionCache(GetCurrentProcess(), memory, code_len);
    return memory;
}

uint8_t* build_kbo_cbt_draft_order_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x49, 0xB8,                         /* mov r8, original_trampoline */
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                         /* mov rax, wrapper */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                         /* jmp rax */
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_cbt_draft_order_create_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
