#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../fa_salary_snapshot/state/salary_snapshot_state.h"
#include "../allocation/hook_stubs_near_code.h"
#include "../../patch_helpers/patch_helpers.h"
#include "hook_stubs_season_phase.h"
static int kbo_season_phase_event_stub_append_original_write(uint8_t* code, size_t* used, uint8_t base_reg, uint8_t value)
{
    if (code == NULL || used == NULL) {
        return 0;
    }

    size_t n = *used;
    switch (base_reg) {
    case 1: code[n++] = 0xC6; code[n++] = 0x81; break;             /* rcx */
    case 2: code[n++] = 0xC6; code[n++] = 0x82; break;             /* rdx */
    case 3: code[n++] = 0xC6; code[n++] = 0x83; break;             /* rbx */
    case 6: code[n++] = 0xC6; code[n++] = 0x86; break;             /* rsi */
    case 7: code[n++] = 0xC6; code[n++] = 0x87; break;             /* rdi */
    case 14: code[n++] = 0x41; code[n++] = 0xC6; code[n++] = 0x86; break; /* r14 */
    case 15: code[n++] = 0x41; code[n++] = 0xC6; code[n++] = 0x87; break; /* r15 */
    default: return 0;
    }

    write_u32(&code[n], OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    n += 4;
    code[n++] = value;
    *used = n;
    return 1;
}

static int kbo_season_phase_event_stub_append_mov_rdx_base(uint8_t* code, size_t* used, uint8_t base_reg)
{
    if (code == NULL || used == NULL) {
        return 0;
    }

    size_t n = *used;
    switch (base_reg) {
    case 1: code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xCA; break; /* mov rdx, rcx */
    case 2: break;                                                       /* rdx already holds base */
    case 3: code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xDA; break; /* mov rdx, rbx */
    case 6: code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xF2; break; /* mov rdx, rsi */
    case 7: code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xFA; break; /* mov rdx, rdi */
    case 14: code[n++] = 0x4C; code[n++] = 0x89; code[n++] = 0xF2; break; /* mov rdx, r14 */
    case 15: code[n++] = 0x4C; code[n++] = 0x89; code[n++] = 0xFA; break; /* mov rdx, r15 */
    default: return 0;
    }

    *used = n;
    return 1;
}

uint8_t* build_kbo_season_phase_opening_day_event_stub(
    void* patch_site,
    uint8_t base_reg,
    uint32_t site_rva,
    uint8_t value)
{
    uint8_t code[160] = {0};
    size_t n = 0;

    code[n++] = 0x9C; /* pushfq */
    code[n++] = 0x50; /* push rax */
    code[n++] = 0x51; /* push rcx */
    code[n++] = 0x52; /* push rdx */

    if (!kbo_season_phase_event_stub_append_original_write(code, &n, base_reg, value)
            || !kbo_season_phase_event_stub_append_mov_rdx_base(code, &n, base_reg)) {
        return NULL;
    }

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&g_kbo_fa_salary_snapshot_phase_event_write_cursor);
    n += 8;
    code[n++] = 0xB9;
    write_u32(&code[n], 1u);
    n += 4;
    code[n++] = 0xF0; code[n++] = 0x0F; code[n++] = 0xC1; code[n++] = 0x08; /* lock xadd dword ptr [rax], ecx */
    code[n++] = 0x83; code[n++] = 0xE1; code[n++] = (uint8_t)KBO_FA_SALARY_PHASE_EVENT_RING_MASK;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)g_kbo_fa_salary_snapshot_phase_event_league_ptrs);
    n += 8;
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x14; code[n++] = 0xC8; /* mov [rax+rcx*8], rdx */

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)g_kbo_fa_salary_snapshot_phase_event_values);
    n += 8;
    code[n++] = 0xC7; code[n++] = 0x04; code[n++] = 0x88;
    write_u32(&code[n], (uint32_t)value);
    n += 4;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)g_kbo_fa_salary_snapshot_phase_event_site_rvas);
    n += 8;
    code[n++] = 0xC7; code[n++] = 0x04; code[n++] = 0x88;
    write_u32(&code[n], site_rva);
    n += 4;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&g_kbo_fa_salary_snapshot_phase_event_published_sequence);
    n += 8;
    code[n++] = 0xF0; code[n++] = 0xFF; code[n++] = 0x00; /* lock inc dword ptr [rax] */

    code[n++] = 0x5A; /* pop rdx */
    code[n++] = 0x59; /* pop rcx */
    code[n++] = 0x58; /* pop rax */
    code[n++] = 0x9D; /* popfq */
    code[n++] = 0xC3; /* ret */

    uint8_t* memory = kbo_alloc_near_code(patch_site, n);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, n);
    FlushInstructionCache(GetCurrentProcess(), memory, n);
    return memory;
}

