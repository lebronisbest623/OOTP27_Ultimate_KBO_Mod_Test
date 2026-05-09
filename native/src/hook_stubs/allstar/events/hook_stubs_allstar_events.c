#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../allocation/hook_stubs_near_code.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "hook_stubs_allstar_events.h"

static void emit_restore_volatile(uint8_t* code, size_t* n)
{
    code[(*n)++] = 0x48; code[(*n)++] = 0x83; code[(*n)++] = 0xC4; code[(*n)++] = 0x28; /* add rsp, 0x28 */
    code[(*n)++] = 0x41; code[(*n)++] = 0x5B;                                           /* pop r11 */
    code[(*n)++] = 0x41; code[(*n)++] = 0x5A;                                           /* pop r10 */
    code[(*n)++] = 0x41; code[(*n)++] = 0x59;                                           /* pop r9 */
    code[(*n)++] = 0x41; code[(*n)++] = 0x58;                                           /* pop r8 */
    code[(*n)++] = 0x5A;                                                               /* pop rdx */
    code[(*n)++] = 0x59;                                                               /* pop rcx */
    code[(*n)++] = 0x58;                                                               /* pop rax */
}

static void emit_abs_jmp(uint8_t* code, size_t* n, void* target)
{
    code[(*n)++] = 0x48; code[(*n)++] = 0xB8;                                           /* mov rax, target */
    write_u64(&code[*n], (uint64_t)(uintptr_t)target);
    *n += 8;
    code[(*n)++] = 0xFF; code[(*n)++] = 0xE0;                                           /* jmp rax */
}

uint8_t* build_allstar_prep_single_division_gate_stub(
    void* patch_site,
    void* return_address,
    void* skip_address)
{
    uint8_t code[160] = {0};
    size_t n = 0;

    code[n++] = 0x50;                                                                   /* push rax */
    code[n++] = 0x51;                                                                   /* push rcx */
    code[n++] = 0x52;                                                                   /* push rdx */
    code[n++] = 0x41; code[n++] = 0x50;                                                 /* push r8 */
    code[n++] = 0x41; code[n++] = 0x51;                                                 /* push r9 */
    code[n++] = 0x41; code[n++] = 0x52;                                                 /* push r10 */
    code[n++] = 0x41; code[n++] = 0x53;                                                 /* push r11 */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xEC; code[n++] = 0x28;             /* sub rsp, 0x28 */
    code[n++] = 0x49; code[n++] = 0x8B; code[n++] = 0xCD;                               /* mov rcx, r13 */
    code[n++] = 0x48; code[n++] = 0xB8;                                                 /* mov rax, helper */
    write_u64(&code[n], (uint64_t)(uintptr_t)&ootp_kbo_allow_single_division_allstar_prep);
    n += 8;
    code[n++] = 0xFF; code[n++] = 0xD0;                                                 /* call rax */
    code[n++] = 0x85; code[n++] = 0xC0;                                                 /* test eax, eax */
    code[n++] = 0x0F; code[n++] = 0x85;                                                 /* jne allow_restore */
    size_t allow_rel_offset = n;
    n += 4;

    emit_restore_volatile(code, &n);
    code[n++] = 0xF6; code[n++] = 0x42; code[n++] = 0x4C; code[n++] = 0x01;             /* test byte ptr [rdx+0x4c], 1 */
    code[n++] = 0x75; code[n++] = 0x0C;                                                 /* jne original_clear */
    emit_abs_jmp(code, &n, skip_address);
    emit_abs_jmp(code, &n, return_address);

    size_t allow_restore = n;
    emit_restore_volatile(code, &n);
    emit_abs_jmp(code, &n, skip_address);

    int32_t rel = (int32_t)(allow_restore - (allow_rel_offset + 4));
    write_u32(&code[allow_rel_offset], (uint32_t)rel);

    uint8_t* memory = kbo_alloc_near_code(patch_site, n);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, n);
    FlushInstructionCache(GetCurrentProcess(), memory, n);
    return memory;
}

uint8_t* build_allstar_roster_single_division_gate_stub(
    void* patch_site,
    void* return_address,
    void* skip_address)
{
    uint8_t code[160] = {0};
    size_t n = 0;

    code[n++] = 0x50;                                                                   /* push rax */
    code[n++] = 0x51;                                                                   /* push rcx */
    code[n++] = 0x52;                                                                   /* push rdx */
    code[n++] = 0x41; code[n++] = 0x50;                                                 /* push r8 */
    code[n++] = 0x41; code[n++] = 0x51;                                                 /* push r9 */
    code[n++] = 0x41; code[n++] = 0x52;                                                 /* push r10 */
    code[n++] = 0x41; code[n++] = 0x53;                                                 /* push r11 */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xEC; code[n++] = 0x28;             /* sub rsp, 0x28 */
    code[n++] = 0x49; code[n++] = 0x8B; code[n++] = 0xCD;                               /* mov rcx, r13 */
    code[n++] = 0x48; code[n++] = 0xB8;                                                 /* mov rax, helper */
    write_u64(&code[n], (uint64_t)(uintptr_t)&ootp_kbo_allow_single_division_allstar_roster);
    n += 8;
    code[n++] = 0xFF; code[n++] = 0xD0;                                                 /* call rax */
    code[n++] = 0x85; code[n++] = 0xC0;                                                 /* test eax, eax */
    code[n++] = 0x0F; code[n++] = 0x85;                                                 /* jne allow_restore */
    size_t allow_rel_offset = n;
    n += 4;

    emit_restore_volatile(code, &n);
    code[n++] = 0x41; code[n++] = 0x8B; code[n++] = 0x46; code[n++] = 0x4C;             /* mov eax, [r14+0x4c] */
    code[n++] = 0xA8; code[n++] = 0x01;                                                 /* test al, 1 */
    code[n++] = 0x75; code[n++] = 0x0C;                                                 /* jne original_clear */
    emit_abs_jmp(code, &n, skip_address);
    emit_abs_jmp(code, &n, return_address);

    size_t allow_restore = n;
    emit_restore_volatile(code, &n);
    emit_abs_jmp(code, &n, skip_address);

    int32_t rel = (int32_t)(allow_restore - (allow_rel_offset + 4));
    write_u32(&code[allow_rel_offset], (uint32_t)rel);

    uint8_t* memory = kbo_alloc_near_code(patch_site, n);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, n);
    FlushInstructionCache(GetCurrentProcess(), memory, n);
    return memory;
}
uint8_t* build_allstar_events_prepare_stub(void* return_address, void* allstar_prep_address, uint32_t game_flag_offset)
{
    uint8_t code[73] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x49, 0x8B, 0xCC,                               // mov rcx, r12
        0x48, 0xB8,                                     // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x41, 0x80, 0xBC, 0x24, 0,0,0,0, 0x00,          // cmp byte ptr [r12 + game_flag_offset], 0
        0x75, 0x0C,                                     // jne call_prep
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x49, 0x8B, 0xCC,                               // call_prep: mov rcx, r12
        0x48, 0xB8,                                     // mov rax, allstar_prep_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[9], (uint64_t)(uintptr_t)&ootp_kbo_prepare_allstar_events);
    write_u32(&code[27], game_flag_offset);
    write_u64(&code[36], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[51], (uint64_t)(uintptr_t)allstar_prep_address);
    write_u64(&code[63], (uint64_t)(uintptr_t)return_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

/* reg_idx: 0=R13 (default), 1=R14, 2=R15 */
uint8_t* build_allstar_schedule_import_capture_stub_r(void* return_address, uint32_t game_flag_offset, int reg_idx)
{
    /* mov rcx, Rn  (ModRM byte: CD=R13, CE=R14, CF=R15) */
    static const uint8_t rcx_modrm[3] = {0xCD, 0xCE, 0xCF};
    /* mov byte ptr [Rn+disp32], 1  (ModRM byte: 85=R13, 86=R14, 87=R15) */
    static const uint8_t mov_modrm[3] = {0x85, 0x86, 0x87};
    if (reg_idx < 0 || reg_idx > 2) {
        reg_idx = 0;
    }

    uint8_t code[] = {
        0x50,                                           // [0]  push rax
        0x51,                                           // [1]  push rcx
        0x52,                                           // [2]  push rdx
        0x41, 0x50,                                     // [3]  push r8
        0x41, 0x51,                                     // [5]  push r9
        0x41, 0x52,                                     // [7]  push r10
        0x41, 0x53,                                     // [9]  push r11
        0x48, 0x83, 0xEC, 0x28,                         // [11] sub rsp, 0x28
        0x49, 0x8B, 0xCD,                               // [15] mov rcx, r13  (byte [17] patched below)
        0x48, 0xB8,                                     // [18] mov rax, helper
        0,0,0,0,0,0,0,0,                                // [20] helper address
        0xFF, 0xD0,                                     // [28] call rax
        0x85, 0xC0,                                     // [30] test eax, eax
        0x74, 0x08,                                     // [32] je skip_write
        0x41, 0xC6, 0x85, 0,0,0,0, 0x01,               // [34] mov byte ptr [r13+offset], 1 (byte [36] patched)
        0x48, 0x83, 0xC4, 0x28,                         // [42] skip_write: add rsp, 0x28
        0x41, 0x5B,                                     // [46] pop r11
        0x41, 0x5A,                                     // [48] pop r10
        0x41, 0x59,                                     // [50] pop r9
        0x41, 0x58,                                     // [52] pop r8
        0x5A,                                           // [54] pop rdx
        0x59,                                           // [55] pop rcx
        0x58,                                           // [56] pop rax
        0x48, 0xB8,                                     // [57] mov rax, return_address
        0,0,0,0,0,0,0,0,                                // [59] return address
        0xFF, 0xE0                                      // [67] jmp rax
    };

    code[17] = rcx_modrm[reg_idx];
    code[36] = mov_modrm[reg_idx];
    write_u64(&code[20], (uint64_t)(uintptr_t)&ootp_kbo_capture_allstar_schedule_import_league);
    write_u32(&code[37], game_flag_offset);
    write_u64(&code[59], (uint64_t)(uintptr_t)return_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_allstar_schedule_import_capture_stub(void* return_address, uint32_t game_flag_offset)
{
    return build_allstar_schedule_import_capture_stub_r(return_address, game_flag_offset, 0);
}

uint8_t* build_allstar_voting_begin_prepare_stub(
    void* return_address,
    void* no_game_address,
    void* allstar_team_setup_address,
    uint32_t game_flag_offset)
{
    uint8_t code[91] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x48, 0x8B, 0xCE,                               // mov rcx, rsi
        0x48, 0xBA,                                     // mov rdx, allstar_team_setup_address
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, helper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x28,                         // add rsp, 0x28
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x58,                                           // pop rax
        0x8D, 0x53, 0xE4,                               // lea edx, [rbx - 0x1c]
        0x80, 0xBE, 0,0,0,0, 0x00,                      // cmp byte ptr [rsi + game_flag_offset], 0
        0x74, 0x0C,                                     // je no_game
        0x48, 0xB8,                                     // mov rax, return_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0xB8,                                     // no_game: mov rax, no_game_address
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0                                      // jmp rax
    };

    write_u64(&code[20], (uint64_t)(uintptr_t)allstar_team_setup_address);
    write_u64(&code[30], (uint64_t)(uintptr_t)&ootp_kbo_prepare_allstar_voting_begin);
    write_u32(&code[60], game_flag_offset);
    write_u64(&code[69], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[81], (uint64_t)(uintptr_t)no_game_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_allstar_team_setup_single_division_gate_stub(
    void* return_address,
    void* bail_address)
{
    uint8_t code[116] = {
        0x50,                                           // [0]  push rax
        0x51,                                           // [1]  push rcx
        0x52,                                           // [2]  push rdx
        0x41, 0x50,                                     // [3]  push r8
        0x41, 0x51,                                     // [5]  push r9
        0x41, 0x52,                                     // [7]  push r10
        0x41, 0x53,                                     // [9]  push r11
        0x48, 0x83, 0xEC, 0x28,                         // [11] sub rsp, 0x28
        0x48, 0x8B, 0xCF,                               // [15] mov rcx, rdi
        0x48, 0xB8,                                     // [18] mov rax, helper
        0,0,0,0,0,0,0,0,                                // [20] helper address
        0xFF, 0xD0,                                     // [28] call rax
        0x85, 0xC0,                                     // [30] test eax, eax
        0x75, 0x32,                                     // [32] jne allow_restore

        0x48, 0x83, 0xC4, 0x28,                         // [34] add rsp, 0x28
        0x41, 0x5B,                                     // [38] pop r11
        0x41, 0x5A,                                     // [40] pop r10
        0x41, 0x59,                                     // [42] pop r9
        0x41, 0x58,                                     // [44] pop r8
        0x5A,                                           // [46] pop rdx
        0x59,                                           // [47] pop rcx
        0x58,                                           // [48] pop rax
        0xF6, 0x41, 0x4C, 0x01,                         // [49] test byte ptr [rcx+0x4c], 1
        0x75, 0x11,                                     // [53] jne bail
        0xB9, 0xD8, 0x00, 0x00, 0x00,                   // [55] mov ecx, 0xd8
        0x48, 0xB8,                                     // [60] mov rax, return_address
        0,0,0,0,0,0,0,0,                                // [62] return address
        0xFF, 0xE0,                                     // [70] jmp rax

        0x48, 0xB8,                                     // [72] bail: mov rax, bail_address
        0,0,0,0,0,0,0,0,                                // [74] bail address
        0xFF, 0xE0,                                     // [82] jmp rax

        0x48, 0x83, 0xC4, 0x28,                         // [84] allow_restore: add rsp, 0x28
        0x41, 0x5B,                                     // [88] pop r11
        0x41, 0x5A,                                     // [90] pop r10
        0x41, 0x59,                                     // [92] pop r9
        0x41, 0x58,                                     // [94] pop r8
        0x5A,                                           // [96] pop rdx
        0x59,                                           // [97] pop rcx
        0x58,                                           // [98] pop rax
        0xB9, 0xD8, 0x00, 0x00, 0x00,                   // [99] mov ecx, 0xd8
        0x48, 0xB8,                                     // [104] mov rax, return_address
        0,0,0,0,0,0,0,0,                                // [106] return address
        0xFF, 0xE0                                      // [114] jmp rax
    };

    write_u64(&code[20], (uint64_t)(uintptr_t)&ootp_kbo_allow_single_division_allstar_team_setup);
    write_u64(&code[62], (uint64_t)(uintptr_t)return_address);
    write_u64(&code[74], (uint64_t)(uintptr_t)bail_address);
    write_u64(&code[106], (uint64_t)(uintptr_t)return_address);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

