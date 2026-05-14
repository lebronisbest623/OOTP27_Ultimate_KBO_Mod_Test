#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "../api/league_context_lookup.h"
#include "named_scan/league_named_scan.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../logging/core_log.h"

uintptr_t kbo_find_league_ptr_by_memory_scan(uint32_t league_id)
{
    static uintptr_t cached_ptr = 0;
    static uint32_t cached_league_id = 0;
    static uint32_t cached_id_offset = 0;
    static ULONGLONG last_scan_ms = 0;

    if (cached_ptr != 0
            && cached_league_id == league_id
            && (kbo_league_candidate_matches_id(cached_ptr, league_id, cached_id_offset)
                || kbo_core_named_league_candidate_score(cached_ptr, league_id, NULL, 0u) >= KBO_CORE_NAMED_LEAGUE_SCAN_MIN_SCORE)) {
        return cached_ptr;
    }
    cached_ptr = 0;
    cached_league_id = 0;
    cached_id_offset = 0;

    ULONGLONG now = (ULONGLONG)GetTickCount();
    if (now - last_scan_ms < 10000u) {
        return 0;
    }
    last_scan_ms = now;
    kbo_log_runtimef("KBO league ptr memory scan started league_id=%u", league_id);

    static const uint32_t id_offsets[] = {
        OOTP27_KBO_LEAGUE_ID_OFFSET,
        OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
    };

    uintptr_t address = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)address, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= base) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && mbi.Type == MEM_PRIVATE
                && kbo_league_scan_protect_allows_read(mbi.Protect)
                && mbi.RegionSize >= (SIZE_T)(OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
                && mbi.RegionSize <= (SIZE_T)0x04000000u) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                uint32_t probe = 0;
                SIZE_T bytes_read = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)p, &probe, sizeof(probe), &bytes_read)
                        || bytes_read != sizeof(probe)
                        || probe != league_id) {
                    continue;
                }

                for (size_t i = 0; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if (!kbo_league_candidate_matches_id(candidate, league_id, id_offset)) {
                        continue;
                    }

                    cached_ptr = candidate;
                    cached_league_id = league_id;
                    cached_id_offset = id_offset;
                    kbo_log_runtimef(
                        "KBO league ptr found by memory scan league_id=%u ptr=%p id_offset=0x%x league_year=%u phase=%u phase_year=%u",
                        league_id,
                        (void*)candidate,
                        id_offset,
                        *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET),
                        (unsigned)*(uint8_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_OFFSET),
                        *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET));
                    return candidate;
                }
            }
        }

        address = end;
#if UINTPTR_MAX > 0xffffffffu
        if (address >= (uintptr_t)0x0000800000000000ull) {
            break;
        }
#endif
    }

    kbo_log_runtimef("KBO league ptr memory scan missed league_id=%u", league_id);

    uintptr_t named_ptr = kbo_find_named_league_ptr_by_memory_scan_all(league_id);
    if (named_ptr != 0u) {
        cached_ptr = named_ptr;
        cached_league_id = league_id;
        cached_id_offset = *(uint32_t*)(named_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET) == league_id
            ? OOTP27_KBO_LEAGUE_ID_OFFSET
            : OOTP27_KBO_LEAGUE_ID_OFFSET + 8u;
        return named_ptr;
    }
    return 0;
}
