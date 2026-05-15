#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "foreign_quota_counts.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../injury/api/foreign_injury.h"
#include "../../replacement_seed/api/foreign_replacement_seed.h"

enum {
    KBO_FOREIGN_ORG_COUNT_CACHE_SIZE = 64,
    KBO_FOREIGN_ORG_COUNT_CACHE_TTL_MS = 1000u,
    KBO_FOREIGN_ORG_SNAPSHOT_MAX_TEAMS = 256,
    KBO_FOREIGN_ORG_PARENT_CACHE_SIZE = 512
};

typedef struct KboForeignOrgCountCacheEntry {
    uint32_t team_id;
    uint32_t foreign_count;
    uint32_t asian_count;
    uint32_t non_asian_count;
    DWORD tick;
} KboForeignOrgCountCacheEntry;

typedef struct KboForeignOrgSnapshotEntry {
    uint32_t team_id;
    uint32_t foreign_count;
    uint32_t asian_count;
    uint32_t non_asian_count;
} KboForeignOrgSnapshotEntry;

static KboForeignOrgCountCacheEntry g_kbo_foreign_org_count_cache[KBO_FOREIGN_ORG_COUNT_CACHE_SIZE];
static KboForeignOrgSnapshotEntry g_kbo_foreign_org_snapshot[KBO_FOREIGN_ORG_SNAPSHOT_MAX_TEAMS];
static int g_kbo_foreign_org_snapshot_count = 0;
static DWORD g_kbo_foreign_org_snapshot_tick = 0u;
static LONG g_kbo_foreign_org_snapshot_lock = 0;
static uint32_t g_kbo_foreign_org_parent_cache_team_ids[KBO_FOREIGN_ORG_PARENT_CACHE_SIZE];
static uint32_t g_kbo_foreign_org_parent_cache_org_ids[KBO_FOREIGN_ORG_PARENT_CACHE_SIZE];

static uint32_t kbo_foreign_org_count_cache_slot(uint32_t team_id)
{
    return (team_id ^ (team_id >> 4)) % KBO_FOREIGN_ORG_COUNT_CACHE_SIZE;
}

static uint32_t kbo_foreign_org_team_id_for_team_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint32_t slot = (team_id * 2654435761u) & (KBO_FOREIGN_ORG_PARENT_CACHE_SIZE - 1u);
    if (g_kbo_foreign_org_parent_cache_team_ids[slot] == team_id) {
        return g_kbo_foreign_org_parent_cache_org_ids[slot];
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    uint32_t org_team_id = team_id;
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            org_team_id = parent_team_id;
        }
    }
    g_kbo_foreign_org_parent_cache_org_ids[slot] = org_team_id;
    g_kbo_foreign_org_parent_cache_team_ids[slot] = team_id;
    return org_team_id;
}

static void kbo_foreign_org_count_cache_store(
    uint32_t team_id,
    uint32_t foreign_count,
    uint32_t asian_count,
    uint32_t non_asian_count,
    DWORD now)
{
    KboForeignOrgCountCacheEntry* cached =
        &g_kbo_foreign_org_count_cache[kbo_foreign_org_count_cache_slot(team_id)];
    cached->team_id = team_id;
    cached->foreign_count = foreign_count;
    cached->asian_count = asian_count;
    cached->non_asian_count = non_asian_count;
    cached->tick = now;
}

static int kbo_foreign_org_count_cache_hit(
    uint32_t team_id,
    DWORD now,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    KboForeignOrgCountCacheEntry* cached =
        &g_kbo_foreign_org_count_cache[kbo_foreign_org_count_cache_slot(team_id)];
    if (cached->team_id != team_id
            || cached->tick == 0u
            || now - cached->tick > KBO_FOREIGN_ORG_COUNT_CACHE_TTL_MS) {
        return 0;
    }

    if (out_foreign_count != NULL) { *out_foreign_count = cached->foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = cached->asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = cached->non_asian_count; }
    return 1;
}

static KboForeignOrgSnapshotEntry* kbo_foreign_org_snapshot_entry(uint32_t team_id)
{
    for (int i = 0; i < g_kbo_foreign_org_snapshot_count; i++) {
        if (g_kbo_foreign_org_snapshot[i].team_id == team_id) {
            return &g_kbo_foreign_org_snapshot[i];
        }
    }
    if (g_kbo_foreign_org_snapshot_count >= KBO_FOREIGN_ORG_SNAPSHOT_MAX_TEAMS) {
        return NULL;
    }
    KboForeignOrgSnapshotEntry* entry = &g_kbo_foreign_org_snapshot[g_kbo_foreign_org_snapshot_count++];
    entry->team_id = team_id;
    entry->foreign_count = 0u;
    entry->asian_count = 0u;
    entry->non_asian_count = 0u;
    return entry;
}

static void kbo_foreign_org_snapshot_add_player(uint32_t team_id, uint32_t player_id, int asian_quota)
{
    if (team_id == 0u) {
        return;
    }
    KboForeignOrgSnapshotEntry* entry = kbo_foreign_org_snapshot_entry(team_id);
    if (entry == NULL || kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
        return;
    }
    entry->foreign_count++;
    if (asian_quota) {
        entry->asian_count++;
    } else {
        entry->non_asian_count++;
    }
}

static void kbo_foreign_org_snapshot_add_unique_player(
    uint32_t* team_ids,
    int* team_count,
    uint32_t team_id,
    uint32_t player_id,
    int asian_quota)
{
    if (team_id == 0u) {
        return;
    }
    for (int i = 0; i < *team_count; i++) {
        if (team_ids[i] == team_id) {
            return;
        }
    }
    if (*team_count >= 3) {
        return;
    }
    team_ids[(*team_count)++] = team_id;
    kbo_foreign_org_snapshot_add_player(team_id, player_id, asian_quota);
}

static int kbo_foreign_org_snapshot_rebuild(DWORD now)
{
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    g_kbo_foreign_org_snapshot_count = 0;
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            continue;
        }

        int asian_quota = kbo_player_is_asian_quota_candidate(player);
        uint32_t team_ids[3] = {0u, 0u, 0u};
        int team_count = 0;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        kbo_foreign_org_snapshot_add_unique_player(
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(current_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player(
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(active_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player(
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(loan_team_id),
            player_id,
            asian_quota);
    }
    g_kbo_foreign_org_snapshot_tick = now;
    return 1;
}

static int kbo_foreign_org_snapshot_get(
    uint32_t team_id,
    DWORD now,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    if (kbo_foreign_org_team_id_for_team_id(team_id) != team_id) {
        return 0;
    }

    while (InterlockedCompareExchange(&g_kbo_foreign_org_snapshot_lock, 1, 0) != 0) {
        Sleep(0);
    }
    if (g_kbo_foreign_org_snapshot_tick == 0u
            || now - g_kbo_foreign_org_snapshot_tick > KBO_FOREIGN_ORG_COUNT_CACHE_TTL_MS) {
        if (!kbo_foreign_org_snapshot_rebuild(now)) {
            InterlockedExchange(&g_kbo_foreign_org_snapshot_lock, 0);
            return 0;
        }
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    for (int i = 0; i < g_kbo_foreign_org_snapshot_count; i++) {
        if (g_kbo_foreign_org_snapshot[i].team_id == team_id) {
            foreign_count = g_kbo_foreign_org_snapshot[i].foreign_count;
            asian_count = g_kbo_foreign_org_snapshot[i].asian_count;
            non_asian_count = g_kbo_foreign_org_snapshot[i].non_asian_count;
            break;
        }
    }
    InterlockedExchange(&g_kbo_foreign_org_snapshot_lock, 0);

    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
    kbo_foreign_org_count_cache_store(team_id, foreign_count, asian_count, non_asian_count, now);
    return 1;
}

void kbo_count_team_asian_quota_probe(
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

    DWORD now = GetTickCount();
    KBO_PROFILE_BEGIN(profile_foreign_org_count);
    if (kbo_foreign_org_count_cache_hit(
            team_id,
            now,
            out_foreign_count,
            out_asian_quota_count,
            out_non_asian_foreign_count)) {
        KBO_PROFILE_END(profile_foreign_org_count, "foreign_policy.org_count.cache_hit");
        return;
    }
    if (kbo_foreign_org_snapshot_get(
            team_id,
            now,
            out_foreign_count,
            out_asian_quota_count,
            out_non_asian_foreign_count)) {
        KBO_PROFILE_END(profile_foreign_org_count, "foreign_policy.org_count.snapshot_hit");
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        KBO_PROFILE_END(profile_foreign_org_count, "foreign_policy.org_count.no_vector");
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
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
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
            if (slot <= 20) {
                kbo_log_runtimef(
                    "foreign replacement player seed excluded from org count team=%u player=%u key_slot=%s nation=%u",
                    team_id,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    replacement_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular",
                    nation_id);
            } else if (slot == 21) {
                kbo_log_runtime_line("foreign replacement player seed org-count exclusion log suppressed after 20 entries");
            }
            continue;
        }
        if (kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
            continue;
        }
        foreign_count++;
        if (kbo_player_is_asian_quota_candidate(player)) {
            asian_count++;
        } else {
            non_asian_count++;
        }
    }

    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
    kbo_foreign_org_count_cache_store(team_id, foreign_count, asian_count, non_asian_count, now);
    KBO_PROFILE_END(profile_foreign_org_count, "foreign_policy.org_count.scanned");
}

void kbo_count_team_asian_quota_probe_fresh(
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
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (!kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
            continue;
        }
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            continue;
        }
        if (kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
            continue;
        }
        foreign_count++;
        if (kbo_player_is_asian_quota_candidate(player)) {
            asian_count++;
        } else {
            non_asian_count++;
        }
    }

    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
}

uint32_t kbo_effective_foreign_count_with_asian_quota(uint32_t asian_count, uint32_t non_asian_foreign_count)
{
    return non_asian_foreign_count + (asian_count > 0u ? asian_count - 1u : 0u);
}

void kbo_count_active_asian_quota_by_position(
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
    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
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
        if (!kbo_player_is_asian_quota_candidate(player)) {
            continue;
        }
        if (kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
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

void kbo_count_active_foreign_for_asian_quota(
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
    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
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
        if (kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
            continue;
        }
        int pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
        int asian = kbo_player_is_asian_quota_candidate(player);
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

int kbo_team_active_roster_contains_player(uintptr_t team_ptr, uint32_t player_id)
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
