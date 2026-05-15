#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../hook_stubs/season_phase/hook_stubs_season_phase.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"
#include "patch_installers_season_phase_probe.h"

typedef struct KboSeasonPhaseCaptureSite {
    const char* label;
    uint32_t site_rva;
    uint8_t base_reg;
    uint8_t value;
} KboSeasonPhaseCaptureSite;

static int kbo_build_phase_write_expected(uint8_t base_reg, uint8_t value, uint8_t* out, size_t* out_size)
{
    if (out == NULL || out_size == NULL) {
        return 0;
    }

    size_t n = 0;
    switch (base_reg) {
    case 0: out[n++] = 0xC6; out[n++] = 0x80; break;             /* rax */
    case 1: out[n++] = 0xC6; out[n++] = 0x81; break;             /* rcx */
    case 2: out[n++] = 0xC6; out[n++] = 0x82; break;             /* rdx */
    case 3: out[n++] = 0xC6; out[n++] = 0x83; break;             /* rbx */
    case 6: out[n++] = 0xC6; out[n++] = 0x86; break;             /* rsi */
    case 7: out[n++] = 0xC6; out[n++] = 0x87; break;             /* rdi */
    case 14: out[n++] = 0x41; out[n++] = 0xC6; out[n++] = 0x86; break; /* r14 */
    case 15: out[n++] = 0x41; out[n++] = 0xC6; out[n++] = 0x87; break; /* r15 */
    default: return 0;
    }

    write_u32(&out[n], OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    n += 4;
    out[n++] = value;
    *out_size = n;
    return 1;
}

static int install_kbo_season_phase_capture_hook_patch(const KboSeasonPhaseCaptureSite* site)
{
    if (site == NULL) {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtimef("%s skipped reason=no_exe", site->label);
        return 0;
    }

    uint8_t expected[8] = {0};
    size_t expected_size = 0;
    if (!kbo_build_phase_write_expected(site->base_reg, site->value, expected, &expected_size)) {
        kbo_log_runtimef(
            "%s skipped rva=0x%x reason=unsupported_base base=%u value=%u",
            site->label,
            site->site_rva,
            (unsigned)site->base_reg,
            (unsigned)site->value);
        return 0;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        site->site_rva,
        expected,
        expected_size,
        site->label);
    if (target == NULL) {
        return 0;
    }
    if (!memory_range_readable(target, expected_size)) {
        kbo_log_runtimef("%s skipped target=%p reason=unreadable", site->label, target);
        return 0;
    }

    if (target[0] == 0xE8) {
        kbo_log_runtimef("%s already installed target=%p", site->label, target);
        return 1;
    }

    if (memcmp(target, expected, expected_size) != 0) {
        log_patch_bytes_mismatch(site->label, target, expected_size);
        return 0;
    }

    uint8_t* stub = build_kbo_season_phase_capture_event_stub(
        target,
        site->base_reg,
        site->site_rva,
        site->value);
    if (stub == NULL) {
        kbo_log_runtimef("%s skipped target=%p reason=stub_alloc_failed", site->label, target);
        return 0;
    }

    intptr_t rel = (intptr_t)stub - ((intptr_t)target + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) {
        kbo_log_runtimef("%s skipped target=%p stub=%p reason=stub_too_far", site->label, target, stub);
        return 0;
    }

    uint8_t patch[8] = {0};
    patch[0] = 0xE8;
    write_u32(&patch[1], (uint32_t)(int32_t)rel);
    for (size_t i = 5; i < expected_size && i < sizeof(patch); i++) {
        patch[i] = 0x90;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, expected_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("%s VirtualProtect failed target=%p error=%lu", site->label, target, GetLastError());
        return 0;
    }

    memcpy(target, patch, expected_size);
    FlushInstructionCache(GetCurrentProcess(), target, expected_size);

    DWORD ignored = 0;
    VirtualProtect(target, expected_size, old_protect, &ignored);

    kbo_log_runtimef(
        "%s installed target=%p stub=%p rva=0x%x base=%u value=%u",
        site->label,
        target,
        stub,
        site->site_rva,
        (unsigned)site->base_reg,
        (unsigned)site->value);
    return 1;
}

int install_kbo_season_phase_capture_hooks(void)
{
    static const KboSeasonPhaseCaptureSite sites[] = {
        { "KBO season phase capture init reset first league", OOTP27_SEASON_PHASE_INIT_0A_RVA, 0, 0 },
        { "KBO season phase capture init reset league", OOTP27_SEASON_PHASE_INIT_0B_RVA, 3, 0 },
        { "KBO season phase capture init preseason", OOTP27_SEASON_PHASE_INIT_2_RVA, 3, 2 },
        { "KBO season phase capture init reset finalize", OOTP27_SEASON_PHASE_INIT_0C_RVA, 3, 0 },
        { "KBO season phase capture offseason started", OOTP27_SEASON_PHASE_WRITE_1_RVA, 15, 1 },
        { "KBO season phase capture offseason reset", OOTP27_SEASON_PHASE_WRITE_0_RVA, 15, 0 },
        { "KBO season phase capture postseason", OOTP27_SEASON_PHASE_WRITE_4_RVA, 15, 4 },
        { "KBO season phase capture preseason a", OOTP27_SEASON_PHASE_WRITE_2A_RVA, 14, 2 },
        { "KBO season phase capture preseason b", OOTP27_SEASON_PHASE_WRITE_2B_RVA, 3, 2 },
        { "KBO season phase capture preseason c", OOTP27_SEASON_PHASE_WRITE_2C_RVA, 2, 2 },
        { "KBO season phase capture regular a", OOTP27_SEASON_PHASE_WRITE_3A_RVA, 7, 3 },
        { "KBO season phase capture regular b", OOTP27_SEASON_PHASE_WRITE_3B_RVA, 1, 3 },
        { "KBO season phase capture regular c", OOTP27_SEASON_PHASE_WRITE_3C_RVA, 6, 3 },
    };

    int ok = 1;
    for (size_t i = 0; i < sizeof(sites) / sizeof(sites[0]); i++) {
        ok &= install_kbo_season_phase_capture_hook_patch(&sites[i]);
    }

    kbo_log_runtimef(
        "KBO season phase capture hooks install complete ok=%d sites=%u",
        ok,
        (unsigned)(sizeof(sites) / sizeof(sites[0])));
    return ok;
}

int install_kbo_season_phase_opening_day_snapshot_hooks(void)
{
    return install_kbo_season_phase_capture_hooks();
}
