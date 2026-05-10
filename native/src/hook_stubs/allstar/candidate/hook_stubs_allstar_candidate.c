#include "hook_stubs_allstar_candidate.h"
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

uint8_t* build_allstar_candidate_team_split_stub(
    void* return_address,
    void* seeded_address,
    void* vector_push_back_address,
    uint32_t subleague_array_offset,
    uint32_t subleague_count_offset)
{
    uint8_t code[96] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x49, 0x8B, 0xCE,                               // mov rcx, r14
        0x48, 0x8D, 0x55, 0xF0,                         // lea rdx, [rbp - 0x10]
        0x4C, 0x8D, 0x45, 0x10,                         // lea r8, [rbp + 0x10]
        0x49, 0xB9,                                     // mov r9, vector_push_back_address
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x85, 0xC0,                                     // test eax, eax
        0x75, 0x27,                                     // jne seeded_path
        0x45, 0x39, 0xAE, 0,0,0,0,                      // cmp dword ptr [r14 + subleague_count_offset], r13d
        0x7E, 0x0C,                                     // jle no_subleague
        0x49, 0x8B, 0x86, 0,0,0,0,                      // mov rax, qword ptr [r14 + subleague_array_offset]
        0x4C, 0x8B, 0x30,                               // mov r14, qword ptr [rax]
        0xEB, 0x03,                                     // jmp fallback_done
        0x4D, 0x8B, 0xF5,                               // no_subleague: mov r14, r13
        0x45, 0x8B, 0xFD,                               // fallback_done: mov r15d, r13d
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0xB8,                                     // seeded_path: mov rax, seeded_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[17], (uint64_t)(uintptr_t)vector_push_back_address);
    write_u64(&code[27], (uint64_t)(uintptr_t)&ootp_kbo_seed_single_division_allstar_candidate_teams);
    write_u32(&code[48], subleague_count_offset);
    write_u32(&code[57], subleague_array_offset);
    write_u64(&code[74], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[86], (uint64_t)(uintptr_t)seeded_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_allstar_candidate_team_roster_push_filter_stub(void* return_address, void* vector_push_back_address)
{
    uint8_t code[57] = {
        0x8B, 0x4C, 0x24, 0x48,                   // mov ecx, [rsp + 0x48]
        0x4D, 0x8B, 0xC6,                         // mov r8, r14
        0x49, 0xB9,                               // mov r9, vector_push_back_address
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                               // mov rax, helper
        0,0,0,0,0,0,0,0,
        0x48, 0x83, 0xEC, 0x28,                   // sub rsp, 0x28
        0xFF, 0xD0,                               // call rax
        0x48, 0x83, 0xC4, 0x28,                   // add rsp, 0x28
        0xFF, 0xC7,                               // inc edi
        0x48, 0xFF, 0xC3,                         // inc rbx
        0x49, 0x8B, 0xCF,                         // mov rcx, r15
        0x48, 0xB8,                               // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                // jmp rax
    };

    write_u64(&code[9], (uint64_t)(uintptr_t)vector_push_back_address);
    write_u64(&code[19], (uint64_t)(uintptr_t)&ootp_kbo_allstar_candidate_push_filter);
    write_u64(&code[47], (uint64_t)(uintptr_t)return_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_allstar_candidate_player_push_filter_stub(void* return_address, void* skip_address, void* vector_push_back_address)
{
    uint8_t code[67] = {
        0x4C, 0x8B, 0xC1,                         // mov r8, rcx
        0x49, 0x8B, 0xD6,                         // mov rdx, r14
        0x8B, 0x4C, 0x24, 0x48,                   // mov ecx, [rsp + 0x48]
        0x49, 0xB9,                               // mov r9, vector_push_back_address
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                               // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                               // call rax
        0x85, 0xC0,                               // test eax, eax
        0x74, 0x13,                               // je skip_path
        0x8B, 0xC3,                               // mov eax, ebx
        0x44, 0x8B, 0x64, 0x24, 0x58,             // mov r12d, [rsp + 0x58]
        0x48, 0xB8,                               // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                               // jmp rax
        0x48, 0xB8,                               // skip_path: mov rax, skip_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                // jmp rax
    };

    write_u64(&code[12], (uint64_t)(uintptr_t)vector_push_back_address);
    write_u64(&code[22], (uint64_t)(uintptr_t)&ootp_kbo_allstar_candidate_push_filter);
    write_u64(&code[45], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[57], (uint64_t)(uintptr_t)skip_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_allstar_candidate_ranked_player_push_filter_stub(
    void* return_address,
    void* skip_address,
    void* vector_push_back_address,
    int vector_base_in_rax)
{
    uint8_t code[80] = {
        0x4D, 0x8B, 0xD0,                         // mov r10, r8
        0x8B, 0x4C, 0x24, 0x48,                   // mov ecx, [rsp + 0x48]
        0x4C, 0x8B, 0xD9,                         // mov r11, rcx
        0x49, 0xC1, 0xE3, 0x05,                   // shl r11, 5
        0x4D, 0x03, 0xD3,                         // add r10, r11
        0x4D, 0x8B, 0xC2,                         // mov r8, r10
        0x48, 0x8B, 0xD3,                         // mov rdx, rbx
        0x49, 0xB9,                               // mov r9, vector_push_back_address
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                               // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                               // call rax
        0x85, 0xC0,                               // test eax, eax
        0x74, 0x13,                               // je skip_path
        0x44, 0x89, 0xAB, 0xE8, 0x0F, 0x00, 0x00, // mov [rbx + 0xfe8], r13d
        0x48, 0xB8,                               // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                               // jmp rax
        0x48, 0xB8,                               // skip_path: mov rax, skip_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                // jmp rax
    };

    if (vector_base_in_rax) {
        code[0] = 0x4C;                            // mov r10, rax
        code[1] = 0x8B;
        code[2] = 0xD0;
    }

    write_u64(&code[25], (uint64_t)(uintptr_t)vector_push_back_address);
    write_u64(&code[35], (uint64_t)(uintptr_t)&ootp_kbo_allstar_candidate_push_filter);
    write_u64(&code[58], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[70], (uint64_t)(uintptr_t)skip_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
