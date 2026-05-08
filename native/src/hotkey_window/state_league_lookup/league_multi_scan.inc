static int kbo_scan_named_league_ptrs_for_ids(const uint32_t* league_ids, int league_count, SIZE_T max_region_size)
{
    if (league_ids == NULL || league_count <= 0) {
        return 0;
    }

    int found = kbo_hub_count_cached_league_ids(league_ids, league_count);
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
                && mbi.RegionSize >= (SIZE_T)KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN
                && mbi.RegionSize <= max_region_size) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                uint32_t probe_id = *(uint32_t*)p;
                if (kbo_hub_league_id_list_index(league_ids, league_count, probe_id) < 0) {
                    continue;
                }

                static const uint32_t id_offsets[] = {
                    OOTP27_KBO_LEAGUE_ID_OFFSET,
                    OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
                };
                for (size_t i = 0; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < base + id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if ((candidate & 7u) != 0
                            || candidate < base
                            || candidate + KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN > end) {
                        continue;
                    }

                    char candidate_name[96] = {0};
                    char candidate_logo[128] = {0};
                    int score = kbo_hub_named_league_candidate_score(
                        candidate,
                        probe_id,
                        candidate_name,
                        sizeof(candidate_name),
                        candidate_logo,
                        sizeof(candidate_logo));
                    if (score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
                        continue;
                    }

                    int before = kbo_hub_find_league_display_cache_slot(probe_id) >= 0;
                    kbo_hub_store_league_display_cache(probe_id, candidate, score, candidate_name, candidate_logo);
                    if (!before && kbo_hub_find_league_display_cache_slot(probe_id) >= 0) {
                        found++;
                        if (found >= league_count) {
                            return found;
                        }
                    }
                }
            }
        }

        address = end;
        if (address >= (uintptr_t)0x0000800000000000ull) {
            break;
        }
    }

    return found;
}

