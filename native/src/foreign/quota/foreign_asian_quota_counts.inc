/* Asian quota organization and active-roster count helpers. Included from foreign_asian_quota.inc. */

static void kbo_count_team_asian_quota_probe(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    if (out_foreign_count != NULL) { *out_foreign_count = 0u; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = 0u; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = 0u; }
    if (team_id == 0u) {
        return;
    }

    enum { KBO_FOREIGN_ORG_COUNT_CACHE_SIZE = 64 };
    typedef struct KboForeignOrgCountCacheEntry {
        uint32_t team_id;
        uint32_t foreign_count;
        uint32_t asian_count;
        uint32_t non_asian_count;
        DWORD tick;
    } KboForeignOrgCountCacheEntry;
    static KboForeignOrgCountCacheEntry count_cache[KBO_FOREIGN_ORG_COUNT_CACHE_SIZE] = {{0}};

    DWORD now = GetTickCount();
    uint32_t slot_index = (team_id ^ (team_id >> 4)) % KBO_FOREIGN_ORG_COUNT_CACHE_SIZE;
    KboForeignOrgCountCacheEntry* cached = &count_cache[slot_index];
    if (cached->team_id == team_id && cached->tick != 0u && now - cached->tick <= 500u) {
        if (out_foreign_count != NULL) { *out_foreign_count = cached->foreign_count; }
        if (out_asian_quota_count != NULL) { *out_asian_quota_count = cached->asian_count; }
        if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = cached->non_asian_count; }
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (!kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
            continue;
        }
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            static volatile LONG replacement_seed_count_log_count = 0;
            LONG slot = InterlockedIncrement(&replacement_seed_count_log_count);
            if (slot <= 120) {
                append_logf(
                    "foreign replacement player seed excluded from org count team=%u player=%u key_slot=%s nation=%u",
                    team_id,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    replacement_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular",
                    nation_id);
            }
            continue;
        }
        foreign_count++;
        if (kbo_nation_is_asian_quota_candidate(nation_id)) {
            asian_count++;
        } else {
            non_asian_count++;
        }
    }

    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
    cached->team_id = team_id;
    cached->foreign_count = foreign_count;
    cached->asian_count = asian_count;
    cached->non_asian_count = non_asian_count;
    cached->tick = now;
}

static uint32_t kbo_effective_foreign_count_with_asian_quota(uint32_t asian_count, uint32_t non_asian_foreign_count)
{
    return non_asian_foreign_count + (asian_count > 0u ? asian_count - 1u : 0u);
}

static void kbo_count_active_asian_quota_by_position(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers)
{
    if (out_asian_hitters != NULL) { *out_asian_hitters = 0u; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = 0u; }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        uint32_t player_id = active_ids[i];
        if (player_id == 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (!kbo_nation_is_asian_quota_candidate(nation_id)) {
            continue;
        }

        if (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u) {
            asian_pitchers++;
        } else {
            asian_hitters++;
        }
    }

    if (out_asian_hitters != NULL) { *out_asian_hitters = asian_hitters; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = asian_pitchers; }
}

static void kbo_count_active_foreign_for_asian_quota(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers,
    uint32_t* out_non_asian_hitters,
    uint32_t* out_non_asian_pitchers)
{
    if (out_asian_hitters != NULL) { *out_asian_hitters = 0u; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = 0u; }
    if (out_non_asian_hitters != NULL) { *out_non_asian_hitters = 0u; }
    if (out_non_asian_pitchers != NULL) { *out_non_asian_pitchers = 0u; }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        uint32_t player_id = active_ids[i];
        if (player_id == 0u) {
            continue;
        }
        uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        int pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
        int asian = kbo_nation_is_asian_quota_candidate(nation_id);
        if (pitcher) {
            if (asian) { asian_pitchers++; } else { non_asian_pitchers++; }
        } else {
            if (asian) { asian_hitters++; } else { non_asian_hitters++; }
        }
    }

    if (out_asian_hitters != NULL) { *out_asian_hitters = asian_hitters; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = asian_pitchers; }
    if (out_non_asian_hitters != NULL) { *out_non_asian_hitters = non_asian_hitters; }
    if (out_non_asian_pitchers != NULL) { *out_non_asian_pitchers = non_asian_pitchers; }
}

static int kbo_team_active_roster_contains_player(uintptr_t team_ptr, uint32_t player_id)
{
    if (team_ptr == 0
            || player_id == 0u
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (active_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

