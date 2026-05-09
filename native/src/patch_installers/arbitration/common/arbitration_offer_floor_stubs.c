#include "arbitration_offer_floor_stubs.h"
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
#include "arbitration_patch_helpers.h"

uint8_t* build_kbo_salary_arbitration_final_zero_tender_stub(
    void* non_tender_continuation,
    void* tender_continuation)
{
    uint8_t code[112] = {
        0x41, 0x83, 0xBD, 0x7C, 0x08, 0x00, 0x00, 0x00, /* cmp dword ptr [r13+0x87c], 0 */
        0x0F, 0x8F, 0x42, 0x00, 0x00, 0x00,             /* jg tender_continuation */
        0x41, 0x83, 0x7D, 0x48, 0x64,                   /* cmp dword ptr [r13+0x48], 100 */
        0x74, 0x21,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBE, 0x20, 0x01, 0x00, 0x00, 0x64, /* cmp dword ptr [r14+0x120], 100 */
        0x74, 0x17,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBD, 0x70, 0x0D, 0x00, 0x00, 0x64, /* cmp dword ptr [r13+0xd70], 100 */
        0x74, 0x0D,                                     /* je clamp_to_floor */
        0x49, 0xBB,                                     /* mov r11, non_tender_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0x45, 0x8B, 0x9C, 0x24, 0x78, 0x02, 0x00, 0x00, /* mov r11d, [r12+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg write_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x45, 0x89, 0x9D, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], r11d */
        0x49, 0xBB,                                     /* mov r11, tender_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[43], (uint64_t)(uintptr_t)non_tender_continuation);
    write_u64(&code[82], (uint64_t)(uintptr_t)tender_continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

uint8_t* build_kbo_salary_arbitration_ai_offer_write_681012_stub(
    void* offer_above_strong_continuation,
    void* offer_not_above_strong_continuation)
{
    uint8_t code[96] = {
        0x41, 0x83, 0x7D, 0x48, 0x64,                   /* cmp dword ptr [r13+0x48], 100 */
        0x75, 0x1B,                                     /* jne write_original */
        0x45, 0x8B, 0x9C, 0x24, 0x78, 0x02, 0x00, 0x00, /* mov r11d, [r12+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg compare_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x41, 0x3B, 0xC3,                               /* cmp eax, r11d */
        0x7D, 0x03,                                     /* jge write_original */
        0x41, 0x8B, 0xC3,                               /* mov eax, r11d */
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], eax */
        0x66, 0x41, 0x0F, 0x2F, 0xF3,                   /* comisd xmm6, xmm11 */
        0x76, 0x0D,                                     /* jbe offer_not_above_strong_continuation */
        0x49, 0xBB,                                     /* mov r11, offer_above_strong_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0x49, 0xBB,                                     /* mov r11, offer_not_above_strong_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[50], (uint64_t)(uintptr_t)offer_above_strong_continuation);
    write_u64(&code[63], (uint64_t)(uintptr_t)offer_not_above_strong_continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

uint8_t* build_kbo_salary_arbitration_ai_offer_write_6827cd_stub(
    void* original_lea_value,
    void* continuation)
{
    uint8_t code[80] = {
        0x83, 0x7B, 0x48, 0x64,                         /* cmp dword ptr [rbx+0x48], 100 */
        0x75, 0x1A,                                     /* jne write_original */
        0x45, 0x8B, 0x9F, 0x78, 0x02, 0x00, 0x00,       /* mov r11d, [r15+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg compare_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x41, 0x3B, 0xC3,                               /* cmp eax, r11d */
        0x7D, 0x03,                                     /* jge write_original */
        0x41, 0x8B, 0xC3,                               /* mov eax, r11d */
        0x89, 0x83, 0x7C, 0x08, 0x00, 0x00,             /* mov [rbx+0x87c], eax */
        0x48, 0xB8,                                     /* mov rax, original_lea_value */
        0,0,0,0,0,0,0,0,
        0x49, 0xBB,                                     /* mov r11, continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[40], (uint64_t)(uintptr_t)original_lea_value);
    write_u64(&code[50], (uint64_t)(uintptr_t)continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

uint8_t* build_kbo_salary_arbitration_zero_offer_check_682089_stub(
    void* non_tender_continuation,
    void* continuation)
{
    uint8_t code[112] = {
        0x41, 0x8B, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov eax, [r13+0x87c] */
        0x85, 0xC0,                                     /* test eax, eax */
        0x75, 0x38,                                     /* jne normal */
        0x41, 0x83, 0x7D, 0x48, 0x64,                   /* cmp dword ptr [r13+0x48], 100 */
        0x74, 0x14,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBE, 0x20, 0x01, 0x00, 0x00, 0x64, /* cmp dword ptr [r14+0x120], 100 */
        0x74, 0x0A,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBD, 0x70, 0x0D, 0x00, 0x00, 0x64, /* cmp dword ptr [r13+0xd70], 100 */
        0x75, 0x31,                                     /* jne non_tender_continuation */
        0x45, 0x8B, 0x9C, 0x24, 0x78, 0x02, 0x00, 0x00, /* mov r11d, [r12+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg write_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x44, 0x89, 0xD8,                               /* mov eax, r11d */
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], eax */
        0x41, 0x8B, 0x8D, 0x80, 0x08, 0x00, 0x00,       /* mov ecx, [r13+0x880] */
        0x49, 0xBB,                                     /* mov r11, continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0x49, 0xBB,                                     /* mov r11, non_tender_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[76], (uint64_t)(uintptr_t)continuation);
    write_u64(&code[89], (uint64_t)(uintptr_t)non_tender_continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}

uint8_t* build_kbo_salary_arbitration_high_limit_non_tender_6820af_stub(
    void* tender_continuation,
    void* non_tender_call_continuation)
{
    uint8_t code[120] = {
        0x48, 0x8B, 0x75, 0xB8,                         /* mov rsi, [rbp-0x48] */
        0x4C, 0x8B, 0x75, 0xC0,                         /* mov r14, [rbp-0x40] */
        0x41, 0x83, 0x7D, 0x48, 0x64,                   /* cmp dword ptr [r13+0x48], 100 */
        0x74, 0x14,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBE, 0x20, 0x01, 0x00, 0x00, 0x64, /* cmp dword ptr [r14+0x120], 100 */
        0x74, 0x0A,                                     /* je clamp_to_floor */
        0x41, 0x83, 0xBD, 0x70, 0x0D, 0x00, 0x00, 0x64, /* cmp dword ptr [r13+0xd70], 100 */
        0x75, 0x35,                                     /* jne non_tender_call */
        0x49, 0x8D, 0xBD, 0x14, 0x09, 0x00, 0x00,       /* lea rdi, [r13+0x914] */
        0x49, 0x8D, 0x9D, 0xA0, 0x08, 0x00, 0x00,       /* lea rbx, [r13+0x8a0] */
        0x45, 0x8B, 0x9C, 0x24, 0x78, 0x02, 0x00, 0x00, /* mov r11d, [r12+0x278] */
        0x45, 0x85, 0xDB,                               /* test r11d, r11d */
        0x7F, 0x06,                                     /* jg write_floor */
        0x41, 0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov r11d, 1 */
        0x45, 0x89, 0x9D, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], r11d */
        0x49, 0xBB,                                     /* mov r11, tender_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0x45, 0x0F, 0xB6, 0xC7,                         /* movzx r8d, r15b */
        0x49, 0x8B, 0xD5,                               /* mov rdx, r13 */
        0x49, 0x8B, 0xCE,                               /* mov rcx, r14 */
        0x49, 0xBB,                                     /* mov r11, non_tender_call_continuation */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               /* jmp r11 */
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC
    };

    write_u64(&code[77], (uint64_t)(uintptr_t)tender_continuation);
    write_u64(&code[100], (uint64_t)(uintptr_t)non_tender_call_continuation);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}
