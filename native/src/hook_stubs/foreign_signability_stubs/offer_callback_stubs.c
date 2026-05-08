#include "offer_callback_stubs.h"
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
#include "../../bootstrap/hook_entrypoints.h"

uint8_t* build_kbo_fa_offer_screen_callback_probe_detour_stub(void* original_trampoline)
{
    uint8_t code[64] = {
        0x48, 0x83, 0xEC, 0x38,                         // sub rsp, 0x38
        0x48, 0xB8,                                     // mov rax, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0x89, 0x44, 0x24, 0x20,                   // mov [rsp+0x20], rax
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x38,                         // add rsp, 0x38
        0xC3,                                           // ret
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[6], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[21], (uint64_t)(uintptr_t)&ootp_kbo_fa_offer_screen_callback_probe_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_fa_contract_offer_callback_probe_detour_stub(void* original_trampoline)
{
    uint8_t code[64] = {
        0x48, 0x83, 0xEC, 0x38,                         // sub rsp, 0x38
        0x48, 0xB8,                                     // mov rax, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0x89, 0x44, 0x24, 0x20,                   // mov [rsp+0x20], rax
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x38,                         // add rsp, 0x38
        0xC3,                                           // ret
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[6], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[21], (uint64_t)(uintptr_t)&ootp_kbo_fa_contract_offer_callback_probe_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_player_action_eligibility_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x49, 0xB9,                                     // mov r9, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_player_action_eligibility_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
