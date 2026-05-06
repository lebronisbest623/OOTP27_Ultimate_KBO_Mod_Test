static uint32_t  g_kbo_league_ptr_cache_id  = 0;
static uintptr_t g_kbo_league_ptr_cache_ptr = 0;
#define KBO_LEAGUE_PTR_MISS_CACHE_MAX 32
static uint32_t  g_kbo_league_ptr_miss_cache_ids[KBO_LEAGUE_PTR_MISS_CACHE_MAX] = {0};
static ULONGLONG g_kbo_league_ptr_miss_cache_until_ms[KBO_LEAGUE_PTR_MISS_CACHE_MAX] = {0};

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

static uintptr_t kbo_find_league_ptr(uint32_t league_id)
{
    if (league_id == 0) {
        return 0;
    }

    ULONGLONG now_ms = GetTickCount64();
    if (kbo_league_ptr_recent_miss(league_id, now_ms)) {
        return 0;
    }

    /* Fast path: cached value still valid */
    if (g_kbo_league_ptr_cache_id == league_id && g_kbo_league_ptr_cache_ptr != 0) {
        uintptr_t cached = g_kbo_league_ptr_cache_ptr;
        if (memory_range_readable((void*)cached, OOTP27_KBO_LEAGUE_ID_OFFSET + 4u)
                && *(uint32_t*)((uint8_t*)cached + OOTP27_KBO_LEAGUE_ID_OFFSET) == league_id) {
            return cached;
        }
        g_kbo_league_ptr_cache_id  = 0;
        g_kbo_league_ptr_cache_ptr = 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        kbo_remember_league_ptr_miss(league_id, now_ms);
        return 0;
    }

    /* Try multiple possible offsets for league vector in global DB. */
    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u, 0xe0u, 0xe8u, 0xf0u, 0xf8u,
        0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };

    uintptr_t team_vector = 0;
    int32_t   team_count  = 0;
    /* Get team vector to use for filtering (we'll skip this in our search) */
    if (kbo_hub_get_team_vector(&team_vector, &team_count)) {
        (void)team_count;
    }

    for (size_t i = 0; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        uint32_t vec_off = league_vec_offsets[i];
        uint32_t cnt_off = vec_off + 8u;  /* Count is usually 8 bytes after vector pointer */
        
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }
        
        uintptr_t candidate_vec = *(uintptr_t*)((uint8_t*)global + vec_off);
        int32_t candidate_count = *(int32_t*)((uint8_t*)global + cnt_off);
        
        if (candidate_vec == 0 || candidate_vec == team_vector) {
            continue; /* skip empty or the team vector */
        }
        if (candidate_count < 1 || candidate_count > 500) {
            continue;
        }
        if (!memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }
        
        /* Scan all entries in this vector for matching league ID */
        for (int32_t j = 0; j < candidate_count; j++) {
            uintptr_t league_candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            if (league_candidate == 0 || !memory_range_readable((void*)league_candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 4u)) {
                continue;
            }
            
            uint32_t cand_id = *(uint32_t*)((uint8_t*)league_candidate + OOTP27_KBO_LEAGUE_ID_OFFSET);
            if (cand_id == league_id) {
                uint32_t cand_year = 0;
                if (memory_range_readable((void*)(league_candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET), 4u)) {
                    cand_year = *(uint32_t*)((uint8_t*)league_candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
                }
                append_logf("KBO: league ptr FOUND id=%u ptr=%p year=%u (offset 0x%x index %d)", 
                    league_id, (void*)league_candidate, cand_year, vec_off, j);
                g_kbo_league_ptr_cache_id  = league_id;
                g_kbo_league_ptr_cache_ptr = league_candidate;
                kbo_forget_league_ptr_miss(league_id);
                return league_candidate;
            }
        }
    }

    kbo_remember_league_ptr_miss(league_id, now_ms);
    return 0;
}
