static uintptr_t kbo_scan_named_league_ptr(uint32_t league_id, SIZE_T max_region_size, int* out_score, char* out_name, size_t out_name_size)
{
    uintptr_t best_ptr = 0;
    int best_score = -1000;
    char best_name[96] = {0};
    char best_logo[128] = {0};

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
                if (*(uint32_t*)p != league_id) {
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
                        league_id,
                        candidate_name,
                        sizeof(candidate_name),
                        candidate_logo,
                        sizeof(candidate_logo));
                    if (score > best_score) {
                        best_score = score;
                        best_ptr = candidate;
                        snprintf(best_name, sizeof(best_name), "%s", candidate_name);
                        snprintf(best_logo, sizeof(best_logo), "%s", candidate_logo);
                        if (score >= KBO_NAMED_LEAGUE_SCAN_EARLY_SCORE) {
                            if (out_score != NULL) {
                                *out_score = best_score;
                            }
                            if (out_name != NULL && out_name_size > 0) {
                                snprintf(out_name, out_name_size, "%s", best_name);
                            }
                            kbo_hub_store_league_display_cache(league_id, best_ptr, best_score, best_name, best_logo);
                            return best_ptr;
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

    if (out_score != NULL) {
        *out_score = best_score;
    }
    if (out_name != NULL && out_name_size > 0) {
        snprintf(out_name, out_name_size, "%s", best_name);
    }
    if (best_ptr != 0 && best_score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        kbo_hub_store_league_display_cache(league_id, best_ptr, best_score, best_name, best_logo);
    }
    return best_ptr;
}

