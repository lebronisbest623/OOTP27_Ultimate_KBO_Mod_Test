#include "hook_stubs_allstar_settings.h"
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

uint8_t* build_allstar_settings_enable_stub(void* return_address, void* checkbox_set_bool_address, uint32_t game_flag_offset)
{
    (void)game_flag_offset;

    uint8_t code[64] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x49, 0x8B, 0xCD,                               // mov rcx, r13
        0x48, 0xB8,                                     // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0xBA, 0x01, 0x00, 0x00, 0x00,                   // mov edx, 1
        0x90, 0x90, 0x90,                               // pad original movzx length
        0x48, 0x8B, 0xCF,                               // mov rcx, rdi
        0x48, 0xB8,                                     // mov rax, checkbox_set_bool_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[9], (uint64_t)(uintptr_t)&ootp_kbo_enable_allstar_setting);
    write_u64(&code[36], (uint64_t)(uintptr_t)checkbox_set_bool_address);
    write_u64(&code[48], (uint64_t)(uintptr_t)return_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
