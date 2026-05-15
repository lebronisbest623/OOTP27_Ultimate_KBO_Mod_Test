#include "../internal/foreign_quota_internal.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "foreign_quota_retention_opportunity_probe.h"

typedef struct KboCustomForeignCandidateCacheEntry {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t today;
    uintptr_t player_ptr;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
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

enum {
    KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE = 4096,
    KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_TTL_MS = 500u
};

static KboCustomForeignCandidateCacheEntry g_kbo_custom_foreign_candidate_cache[KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE];

static uint32_t kbo_custom_foreign_candidate_cache_slot(uint32_t team_id, uint32_t player_id)
{
    uint32_t h = player_id * 2654435761u;
    h ^= team_id * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE - 1u);
}

static void kbo_custom_foreign_candidate_cache_store(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t original_team_id,
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

static int kbo_custom_foreign_candidate_cache_hit(
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
    if (!entry->valid
            || entry->team_id != team_id
            || entry->player_id != candidate_id
            || entry->today != today
            || entry->player_ptr != (uintptr_t)candidate
            || entry->current_team_id != current_team_id
            || entry->active_team_id != active_team_id
            || entry->original_team_id != original_team_id
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

uint32_t kbo_custom_foreign_policy_extra_slots_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }

    if (!kbo_foreign_injury_replacement_enabled()
            || team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0u;
    }

    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (candidate_id == 0u) {
        return 0u;
    }

    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (!kbo_foreign_injury_replacement_in_season_window(
            league_id,
            today,
            "foreign_policy.candidate_extra_slot",
            "candidate_extra_slot")) {
        return 0u;
    }

    uint32_t injured_player_id = 0u;
    if (kbo_player_is_asian_quota_candidate(candidate)
            && kbo_team_has_foreign_injury_slot_for_candidate(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA,
                candidate_id,
                &injured_player_id,
                NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        return 1u;
    }

    if (kbo_team_has_foreign_injury_slot_for_candidate(
            team_id,
            KBO_FOREIGN_INJURY_SLOT_REGULAR,
            candidate_id,
            &injured_player_id,
            NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        return 1u;
    }

    return 0u;
}

int kbo_custom_foreign_policy_can_override_original_block(uint8_t* candidate, uint32_t team_id)
{
    if (candidate == NULL || team_id == 0u || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(candidate + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    return current_team_id == 0u || kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
}

int kbo_custom_foreign_policy_team_allows_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_effective_before != NULL) { *out_effective_before = 0u; }
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT; }
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }

    KBO_PROFILE_BEGIN(profile_custom_candidate);
    if (team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        KBO_PROFILE_END(profile_custom_candidate, "foreign_policy.candidate.invalid");
        return 0;
    }

    uint32_t today = 0u;
    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    uint32_t current_team_id = *(uint32_t*)(candidate + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    int cached_allowed = 0;
    if (candidate_id != 0u
            && kbo_custom_foreign_candidate_cache_hit(
                team_id,
                candidate,
                candidate_id,
                today,
                current_team_id,
                active_team_id,
                original_team_id,
                out_effective_before,
                out_effective_after,
                out_effective_limit,
                out_slot_type,
                out_injured_player_id,
                &cached_allowed)) {
        KBO_PROFILE_END(profile_custom_candidate, cached_allowed
            ? "foreign_policy.candidate.cache_allowed"
            : "foreign_policy.candidate.cache_blocked");
        return cached_allowed;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    KBO_PROFILE_BEGIN(profile_custom_candidate_count);
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    KBO_PROFILE_END(profile_custom_candidate_count, "foreign_policy.candidate.org_count");
    (void)foreign_count;

    uint32_t pending_asian_count = 0u;
    uint32_t pending_non_asian_count = 0u;
    int candidate_pending = 0;
    KBO_PROFILE_BEGIN(profile_custom_candidate_pending);
    kbo_custom_foreign_count_pending_offers(
        team_id,
        today,
        candidate_id,
        &pending_asian_count,
        &pending_non_asian_count,
        &candidate_pending);
    KBO_PROFILE_END(profile_custom_candidate_pending, "foreign_policy.candidate.pending");
    asian_count += pending_asian_count;
    non_asian_count += pending_non_asian_count;

    uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    uint32_t asian_after = asian_count;
    uint32_t non_asian_after = non_asian_count;
    int already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
    int retained_by_team = today != 0u
        && candidate_id != 0u
        && kbo_has_active_foreign_waiver_right(team_id, candidate_id, today);
    int counts_as_existing_candidate = already_in_org || retained_by_team;
    if (!counts_as_existing_candidate && !candidate_pending) {
        if (kbo_player_is_asian_quota_candidate(candidate)) {
            asian_after++;
        } else {
            non_asian_after++;
        }
    }

    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    KBO_PROFILE_BEGIN(profile_custom_candidate_extra);
    uint32_t extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
        team_id,
        candidate,
        &slot_type,
        &injured_player_id);
    KBO_PROFILE_END(profile_custom_candidate_extra, "foreign_policy.candidate.extra_slots");
    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT + extra_slots;

    if (out_effective_before != NULL) { *out_effective_before = effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
    if (out_slot_type != NULL) { *out_slot_type = slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }

    int allowed = counts_as_existing_candidate || effective_after <= effective_limit;
    int opportunity_block = kbo_retention_opportunity_probe_should_block(
        team_id,
        candidate,
        today,
        asian_count,
        non_asian_count,
        pending_asian_count,
        pending_non_asian_count,
        asian_after,
        non_asian_after,
        effective_before,
        effective_after,
        effective_limit,
        candidate_pending,
        counts_as_existing_candidate,
        allowed);
    if (allowed && opportunity_block) {
        allowed = 0;
    }
    if (candidate_id != 0u) {
        kbo_custom_foreign_candidate_cache_store(
            team_id,
            candidate,
            candidate_id,
            today,
            current_team_id,
            active_team_id,
            original_team_id,
            InterlockedCompareExchange(&g_kbo_custom_foreign_pending_offer_generation, 0, 0),
            effective_before,
            effective_after,
            effective_limit,
            slot_type,
            injured_player_id,
            allowed);
    }
    KBO_PROFILE_END(profile_custom_candidate, allowed
        ? "foreign_policy.candidate.allowed"
        : "foreign_policy.candidate.blocked");
    return allowed;
}

int kbo_custom_foreign_policy_team_allows_final_signing(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_effective_before != NULL) { *out_effective_before = 0u; }
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT; }
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }

    if (team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe_fresh(team_id, &foreign_count, &asian_count, &non_asian_count);
    (void)foreign_count;

    uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    uint32_t asian_after = asian_count;
    uint32_t non_asian_after = non_asian_count;
    int already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
    if (!already_in_org) {
        if (kbo_player_is_asian_quota_candidate(candidate)) {
            asian_after++;
        } else {
            non_asian_after++;
        }
    }

    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
        team_id,
        candidate,
        &slot_type,
        &injured_player_id);
    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT + extra_slots;

    if (out_effective_before != NULL) { *out_effective_before = effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
    if (out_slot_type != NULL) { *out_slot_type = slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }

    if (already_in_org) {
        return 1;
    }

    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    int allowed = effective_after <= effective_limit;
    int opportunity_block = kbo_retention_opportunity_probe_should_block(
        team_id,
        candidate,
        today,
        asian_count,
        non_asian_count,
        0u,
        0u,
        asian_after,
        non_asian_after,
        effective_before,
        effective_after,
        effective_limit,
        0,
        already_in_org,
        allowed);
    return allowed && !opportunity_block;
}

