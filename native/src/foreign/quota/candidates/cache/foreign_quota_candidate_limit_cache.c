#include "foreign_quota_candidate_limit_cache.h"

typedef struct KboCustomForeignCandidateCacheEntry {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t today;
    uintptr_t player_ptr;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t org_count_generation;
    LONG pending_generation;
    DWORD tick;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint32_t injured_player_id;
    uint8_t slot_type;
    uint8_t allowed;
    uint8_t valid;
} KboCustomForeignCandidateCacheEntry;

typedef struct KboCustomForeignExtraSlotCacheEntry {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t today;
    uint32_t league_id;
    uintptr_t player_ptr;
    int replacement_count;
    DWORD tick;
    uint32_t extra_slots;
    uint32_t injured_player_id;
    uint8_t candidate_asian;
    uint8_t slot_type;
    uint8_t valid;
} KboCustomForeignExtraSlotCacheEntry;

enum {
    KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE = 8192,
    KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_TTL_MS = 5000u,
    KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_SIZE = 4096,
    KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_TTL_MS = 2000u
};

static KboCustomForeignCandidateCacheEntry g_kbo_custom_foreign_candidate_cache[KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE];
static KboCustomForeignExtraSlotCacheEntry
    g_kbo_custom_foreign_extra_slot_cache[KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_SIZE];

static uint32_t kbo_custom_foreign_candidate_cache_slot(uint32_t team_id, uint32_t player_id)
{
    uint32_t h = player_id * 2654435761u;
    h ^= team_id * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE - 1u);
}

static uint32_t kbo_custom_foreign_extra_slot_cache_slot(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t today,
    uint32_t league_id)
{
    uint32_t h = player_id * 2654435761u;
    h ^= team_id * 2246822519u;
    h ^= today * 3266489917u;
    h ^= league_id * 668265263u;
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_SIZE - 1u);
}

void kbo_custom_foreign_candidate_cache_store(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t original_team_id,
    uint32_t org_count_generation,
    LONG pending_generation,
    uint32_t effective_before,
    uint32_t effective_after,
    uint32_t effective_limit,
    uint8_t slot_type,
    uint32_t injured_player_id,
    int allowed)
{
    KboCustomForeignCandidateCacheEntry* entry =
        &g_kbo_custom_foreign_candidate_cache[kbo_custom_foreign_candidate_cache_slot(team_id, candidate_id)];
    entry->valid = 0u;
    entry->team_id = team_id;
    entry->player_id = candidate_id;
    entry->today = today;
    entry->player_ptr = (uintptr_t)candidate;
    entry->current_team_id = current_team_id;
    entry->active_team_id = active_team_id;
    entry->original_team_id = original_team_id;
    entry->org_count_generation = org_count_generation;
    entry->pending_generation = pending_generation;
    entry->effective_before = effective_before;
    entry->effective_after = effective_after;
    entry->effective_limit = effective_limit;
    entry->slot_type = slot_type;
    entry->injured_player_id = injured_player_id;
    entry->allowed = allowed ? 1u : 0u;
    entry->tick = GetTickCount();
    entry->valid = 1u;
}

int kbo_custom_foreign_extra_slot_cache_hit(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t league_id,
    uint8_t candidate_asian,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_extra_slots)
{
    KboCustomForeignExtraSlotCacheEntry* entry =
        &g_kbo_custom_foreign_extra_slot_cache[
            kbo_custom_foreign_extra_slot_cache_slot(team_id, candidate_id, today, league_id)];
    DWORD now = GetTickCount();
    int replacement_count = g_kbo_foreign_injury_replacement_count;
    if (!entry->valid
            || entry->team_id != team_id
            || entry->player_id != candidate_id
            || entry->today != today
            || entry->league_id != league_id
            || entry->player_ptr != (uintptr_t)candidate
            || entry->candidate_asian != candidate_asian
            || entry->replacement_count != replacement_count
            || entry->tick == 0u
            || now - entry->tick > KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_TTL_MS) {
        return 0;
    }

    if (out_slot_type != NULL) { *out_slot_type = entry->slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = entry->injured_player_id; }
    if (out_extra_slots != NULL) { *out_extra_slots = entry->extra_slots; }
    return 1;
}

void kbo_custom_foreign_extra_slot_cache_store(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t league_id,
    uint8_t candidate_asian,
    uint32_t extra_slots,
    uint8_t slot_type,
    uint32_t injured_player_id)
{
    if (team_id == 0u || candidate == NULL || candidate_id == 0u) {
        return;
    }

    KboCustomForeignExtraSlotCacheEntry* entry =
        &g_kbo_custom_foreign_extra_slot_cache[
            kbo_custom_foreign_extra_slot_cache_slot(team_id, candidate_id, today, league_id)];
    entry->valid = 0u;
    entry->team_id = team_id;
    entry->player_id = candidate_id;
    entry->today = today;
    entry->league_id = league_id;
    entry->player_ptr = (uintptr_t)candidate;
    entry->replacement_count = g_kbo_foreign_injury_replacement_count;
    entry->tick = GetTickCount();
    entry->extra_slots = extra_slots;
    entry->candidate_asian = candidate_asian;
    entry->slot_type = slot_type;
    entry->injured_player_id = injured_player_id;
    entry->valid = 1u;
}

int kbo_custom_foreign_candidate_cache_hit(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t original_team_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    int* out_allowed)
{
    KboCustomForeignCandidateCacheEntry* entry =
        &g_kbo_custom_foreign_candidate_cache[kbo_custom_foreign_candidate_cache_slot(team_id, candidate_id)];
    DWORD now = GetTickCount();
    LONG pending_generation = InterlockedCompareExchange(&g_kbo_custom_foreign_pending_offer_generation, 0, 0);
    uint32_t org_count_generation = kbo_foreign_org_count_cache_generation_for_team(team_id);
    if (!entry->valid
            || entry->team_id != team_id
            || entry->player_id != candidate_id
            || entry->today != today
            || entry->player_ptr != (uintptr_t)candidate
            || entry->current_team_id != current_team_id
            || entry->active_team_id != active_team_id
            || entry->original_team_id != original_team_id
            || entry->org_count_generation != org_count_generation
            || entry->pending_generation != pending_generation
            || now - entry->tick > KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_TTL_MS) {
        return 0;
    }

    if (out_effective_before != NULL) { *out_effective_before = entry->effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = entry->effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = entry->effective_limit; }
    if (out_slot_type != NULL) { *out_slot_type = entry->slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = entry->injured_player_id; }
    if (out_allowed != NULL) { *out_allowed = entry->allowed ? 1 : 0; }
    return 1;
}

