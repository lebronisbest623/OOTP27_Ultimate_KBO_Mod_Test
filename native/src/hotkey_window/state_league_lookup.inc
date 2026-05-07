static uint32_t  g_kbo_league_ptr_cache_id  = 0;
static uintptr_t g_kbo_league_ptr_cache_ptr = 0;
#define KBO_LEAGUE_PTR_MISS_CACHE_MAX 32
static uint32_t  g_kbo_league_ptr_miss_cache_ids[KBO_LEAGUE_PTR_MISS_CACHE_MAX] = {0};
static ULONGLONG g_kbo_league_ptr_miss_cache_until_ms[KBO_LEAGUE_PTR_MISS_CACHE_MAX] = {0};

#define KBO_NAMED_LEAGUE_SCAN_MIN_SCORE 90
#define KBO_NAMED_LEAGUE_SCAN_EARLY_SCORE 115
#define KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN (OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
#define KBO_LEAGUE_DISPLAY_CACHE_MAX 64

typedef struct KboHubLeagueDisplayCacheEntry {
    uint32_t league_id;
    uintptr_t league_ptr;
    uint32_t year;
    int score;
    char name[96];
    char logo_file[128];
} KboHubLeagueDisplayCacheEntry;

static uintptr_t g_kbo_league_display_cache_global = 0;
static uintptr_t g_kbo_league_display_cache_prewarmed_global = 0;
static KboHubLeagueDisplayCacheEntry g_kbo_league_display_cache[KBO_LEAGUE_DISPLAY_CACHE_MAX];

static void kbo_hub_clear_league_display_cache(void)
{
    memset(g_kbo_league_display_cache, 0, sizeof(g_kbo_league_display_cache));
    memset(g_kbo_league_ptr_miss_cache_ids, 0, sizeof(g_kbo_league_ptr_miss_cache_ids));
    memset(g_kbo_league_ptr_miss_cache_until_ms, 0, sizeof(g_kbo_league_ptr_miss_cache_until_ms));
    g_kbo_league_ptr_cache_id = 0;
    g_kbo_league_ptr_cache_ptr = 0;
    g_kbo_league_display_cache_prewarmed_global = 0;
}

static void kbo_hub_refresh_league_cache_context(void)
{
    uintptr_t global = get_ootp_global_database();
    if (global != g_kbo_league_display_cache_global) {
        g_kbo_league_display_cache_global = global;
        kbo_hub_clear_league_display_cache();
    }
}

static int kbo_hub_find_league_display_cache_slot(uint32_t league_id)
{
    for (int i = 0; i < KBO_LEAGUE_DISPLAY_CACHE_MAX; i++) {
        if (g_kbo_league_display_cache[i].league_id == league_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_hub_find_league_display_cache_insert_slot(uint32_t league_id)
{
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot >= 0) {
        return slot;
    }

    for (int i = 0; i < KBO_LEAGUE_DISPLAY_CACHE_MAX; i++) {
        if (g_kbo_league_display_cache[i].league_id == 0u) {
            return i;
        }
    }

    return (int)(league_id % KBO_LEAGUE_DISPLAY_CACHE_MAX);
}

static void kbo_hub_store_league_display_cache(
    uint32_t league_id,
    uintptr_t league_ptr,
    int score,
    const char* name,
    const char* logo_file)
{
    if (league_id == 0 || league_ptr == 0 || name == NULL || name[0] == '\0') {
        return;
    }

    int slot = kbo_hub_find_league_display_cache_insert_slot(league_id);
    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->league_id == league_id
            && entry->name[0] != '\0'
            && entry->score > score) {
        return;
    }

    entry->league_id = league_id;
    entry->league_ptr = league_ptr;
    entry->score = score;
    entry->year = 0;
    entry->logo_file[0] = '\0';
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        entry->year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    }
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    if (logo_file != NULL && logo_file[0] != '\0') {
        snprintf(entry->logo_file, sizeof(entry->logo_file), "%s", logo_file);
    }
}

static int kbo_hub_try_copy_cached_league_name(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->name[0] == '\0' || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        return 0;
    }

    snprintf(out, out_size, "%s", entry->name);
    return 1;
}

static int kbo_hub_try_copy_cached_league_logo_file(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->logo_file[0] == '\0' || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        return 0;
    }

    snprintf(out, out_size, "%s", entry->logo_file);
    return 1;
}

static int kbo_hub_try_get_cached_league_ptr(uint32_t league_id, uintptr_t* out_league_ptr)
{
    if (out_league_ptr != NULL) {
        *out_league_ptr = 0;
    }

    kbo_hub_refresh_league_cache_context();
    int slot = kbo_hub_find_league_display_cache_slot(league_id);
    if (slot < 0) {
        return 0;
    }

    KboHubLeagueDisplayCacheEntry* entry = &g_kbo_league_display_cache[slot];
    if (entry->league_ptr == 0
            || entry->score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE
            || !memory_range_readable((void*)entry->league_ptr, KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN)) {
        memset(entry, 0, sizeof(*entry));
        return 0;
    }

    if (out_league_ptr != NULL) {
        *out_league_ptr = entry->league_ptr;
    }
    return 1;
}

static int kbo_league_ptr_recent_miss(uint32_t league_id, ULONGLONG now_ms)
{
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id
                && now_ms < g_kbo_league_ptr_miss_cache_until_ms[i]) {
            return 1;
        }
    }
    return 0;
}

static void kbo_remember_league_ptr_miss(uint32_t league_id, ULONGLONG now_ms)
{
    int slot = -1;
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id) {
            slot = i;
            break;
        }
        if (slot < 0 && (g_kbo_league_ptr_miss_cache_ids[i] == 0u
                || now_ms >= g_kbo_league_ptr_miss_cache_until_ms[i])) {
            slot = i;
        }
    }
    if (slot < 0) {
        slot = (int)(league_id % KBO_LEAGUE_PTR_MISS_CACHE_MAX);
    }
    g_kbo_league_ptr_miss_cache_ids[slot] = league_id;
    g_kbo_league_ptr_miss_cache_until_ms[slot] = now_ms + 3000u;
}

static void kbo_forget_league_ptr_miss(uint32_t league_id)
{
    for (int i = 0; i < KBO_LEAGUE_PTR_MISS_CACHE_MAX; i++) {
        if (g_kbo_league_ptr_miss_cache_ids[i] == league_id) {
            g_kbo_league_ptr_miss_cache_ids[i] = 0;
            g_kbo_league_ptr_miss_cache_until_ms[i] = 0;
        }
    }
}

static int kbo_hub_text_ends_with_ignore_case(const char* text, const char* suffix)
{
    if (text == NULL || suffix == NULL) {
        return 0;
    }

    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if (suffix_len == 0 || text_len < suffix_len) {
        return 0;
    }

    return ascii_equals_ignore_case(text + text_len - suffix_len, suffix);
}

static int kbo_hub_league_name_likeness_score(const char* name)
{
    if (name == NULL) {
        return -100;
    }

    size_t len = strlen(name);
    if (len < 3u || len > 80u) {
        return -100;
    }

    int letters = 0;
    int digits = 0;
    int spaces = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            letters++;
        } else if (c >= '0' && c <= '9') {
            digits++;
        } else if (c == ' ') {
            spaces++;
        } else if (c != '-' && c != '_' && c != '.' && c != '&' && c != '\'') {
            return -50;
        }
    }

    if (letters < 3 || digits > letters / 2) {
        return -50;
    }

    int score = 0;
    if (spaces > 0) {
        score += 12;
    }
    if (len >= 12u) {
        score += 8;
    }
    if (kbo_ascii_contains_ignore_case(name, "League")
            || kbo_ascii_contains_ignore_case(name, "Organization")
            || kbo_ascii_contains_ignore_case(name, "Association")
            || kbo_ascii_contains_ignore_case(name, "Federation")
            || kbo_ascii_contains_ignore_case(name, "Baseball")
            || kbo_ascii_contains_ignore_case(name, "Conference")
            || kbo_ascii_contains_ignore_case(name, "Division")) {
        score += 30;
    }

    return score;
}

static int kbo_hub_abbr_likeness_score(const char* abbr)
{
    if (abbr == NULL) {
        return 0;
    }

    size_t len = strlen(abbr);
    if (len < 2u || len > 12u) {
        return 0;
    }

    int strong = 1;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)abbr[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            strong = 0;
            break;
        }
    }

    return strong ? 15 : 8;
}

static int kbo_hub_named_league_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    char* out_name,
    size_t out_name_size,
    char* out_logo_file,
    size_t out_logo_file_size)
{
    if (out_name != NULL && out_name_size > 0) {
        out_name[0] = '\0';
    }
    if (out_logo_file != NULL && out_logo_file_size > 0) {
        out_logo_file[0] = '\0';
    }
    if (candidate == 0 || league_id == 0
            || !memory_range_readable((void*)candidate, KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN)) {
        return -1000;
    }

    uint32_t primary_id = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_ID_OFFSET);
    uint32_t alternate_id = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    if (primary_id != league_id && alternate_id != league_id) {
        return -1000;
    }

    uint32_t year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (year < 1982u || year > 2200u) {
        return -200;
    }

    char name[96] = {0};
    if (!copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, name, sizeof(name))) {
        return -200;
    }

    int name_score = kbo_hub_league_name_likeness_score(name);
    if (name_score < 30) {
        return -200;
    }

    int score = 0;
    score += (primary_id == league_id) ? 30 : 20;
    score += 20;
    score += name_score;

    uint32_t subleague_count = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_SUBLEAGUE_COUNT_OFFSET);
    if (subleague_count <= 32u) {
        score += 5;
    }

    char abbr[32] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_ABBR_STRING_OFFSET, abbr, sizeof(abbr))) {
        score += kbo_hub_abbr_likeness_score(abbr);
    }

    char logo[128] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_LOGO_STRING_OFFSET, logo, sizeof(logo))
            && (kbo_hub_text_ends_with_ignore_case(logo, ".png")
                || kbo_hub_text_ends_with_ignore_case(logo, ".oi")
                || kbo_hub_text_ends_with_ignore_case(logo, ".jpg")
                || kbo_hub_text_ends_with_ignore_case(logo, ".jpeg"))) {
        score += 6;
        if (out_logo_file != NULL && out_logo_file_size > 0) {
            snprintf(out_logo_file, out_logo_file_size, "%s", logo);
        }
    }

    char stats_path[160] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_STATS_PATH_STRING_OFFSET, stats_path, sizeof(stats_path))
            && kbo_ascii_contains_ignore_case(stats_path, "\\data\\stats\\")) {
        score += 4;
    }

    char schedule_file[128] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_SCHEDULE_FILE_STRING_OFFSET, schedule_file, sizeof(schedule_file))
            && kbo_hub_text_ends_with_ignore_case(schedule_file, ".lsdl")) {
        score += 6;
    }

    if (out_name != NULL && out_name_size > 0) {
        snprintf(out_name, out_name_size, "%s", name);
    }
    return score;
}

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

static int kbo_hub_league_id_list_index(const uint32_t* league_ids, int league_count, uint32_t league_id)
{
    if (league_ids == NULL || league_id == 0) {
        return -1;
    }
    for (int i = 0; i < league_count; i++) {
        if (league_ids[i] == league_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_hub_collect_visible_league_ids(uint32_t* league_ids, int max_leagues)
{
    if (league_ids == NULL || max_leagues <= 0) {
        return 0;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return 0;
    }

    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < max_leagues; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0 || kbo_hub_league_id_list_index(league_ids, league_count, league_id) >= 0) {
            continue;
        }

        league_ids[league_count++] = league_id;
    }

    return league_count;
}

static int kbo_hub_count_cached_league_ids(const uint32_t* league_ids, int league_count)
{
    int count = 0;
    for (int i = 0; i < league_count; i++) {
        if (kbo_hub_find_league_display_cache_slot(league_ids[i]) >= 0) {
            count++;
        }
    }
    return count;
}

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

static void kbo_hub_prewarm_league_display_cache(void)
{
    kbo_hub_refresh_league_cache_context();
    uintptr_t global = g_kbo_league_display_cache_global;
    if (global == 0 || g_kbo_league_display_cache_prewarmed_global == global) {
        return;
    }

    uint32_t league_ids[KBO_LEAGUE_DISPLAY_CACHE_MAX] = {0};
    int league_count = kbo_hub_collect_visible_league_ids(league_ids, KBO_LEAGUE_DISPLAY_CACHE_MAX);
    if (league_count <= 0) {
        return;
    }

    ULONGLONG started_ms = GetTickCount64();
    int found = kbo_scan_named_league_ptrs_for_ids(league_ids, league_count, (SIZE_T)0x00040000u);
    if (found < league_count) {
        found = kbo_scan_named_league_ptrs_for_ids(league_ids, league_count, (SIZE_T)0x00400000u);
    }
    g_kbo_league_display_cache_prewarmed_global = global;
    append_logf(
        "KBO: F2 league cache prewarmed leagues=%d found=%d ms=%llu",
        league_count,
        found,
        (unsigned long long)(GetTickCount64() - started_ms));
}

static uintptr_t kbo_find_league_ptr(uint32_t league_id)
{
    if (league_id == 0) {
        return 0;
    }

    kbo_hub_refresh_league_cache_context();

    ULONGLONG now_ms = GetTickCount64();
    if (kbo_league_ptr_recent_miss(league_id, now_ms)) {
        return 0;
    }

    uintptr_t cached_league_ptr = 0;
    if (kbo_hub_try_get_cached_league_ptr(league_id, &cached_league_ptr)) {
        return cached_league_ptr;
    }

    if (g_kbo_league_ptr_cache_id == league_id && g_kbo_league_ptr_cache_ptr != 0) {
        int cached_score = kbo_hub_named_league_candidate_score(
            g_kbo_league_ptr_cache_ptr,
            league_id,
            NULL,
            0,
            NULL,
            0);
        if (cached_score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
            return g_kbo_league_ptr_cache_ptr;
        }
        g_kbo_league_ptr_cache_id  = 0;
        g_kbo_league_ptr_cache_ptr = 0;
    }

    append_logf("KBO: named league ptr scan started id=%u", league_id);

    int score = -1000;
    char name[96] = {0};
    uintptr_t league_ptr = kbo_scan_named_league_ptr(league_id, (SIZE_T)0x00040000u, &score, name, sizeof(name));
    if (score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        league_ptr = kbo_scan_named_league_ptr(league_id, (SIZE_T)0x00400000u, &score, name, sizeof(name));
    }

    if (league_ptr != 0 && score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        uint32_t year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        append_logf("KBO: named league ptr FOUND id=%u ptr=%p score=%d year=%u name=%s",
            league_id, (void*)league_ptr, score, year, name);
        g_kbo_league_ptr_cache_id  = league_id;
        g_kbo_league_ptr_cache_ptr = league_ptr;
        kbo_forget_league_ptr_miss(league_id);
        return league_ptr;
    }

    append_logf("KBO: named league ptr scan missed id=%u best_score=%d name=%s",
        league_id, score, name[0] != '\0' ? name : "(none)");
    kbo_remember_league_ptr_miss(league_id, now_ms);
    return 0;
}
