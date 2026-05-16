#include "../internal/foreign_quota_internal.h"

#define KBO_CUSTOM_FOREIGN_TRADE_TEAM_ID_OFFSET       0x08u
#define KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET     0x10u
#define KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT           2
#define KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS         10
#define KBO_CUSTOM_FOREIGN_TRADE_READABLE_BYTES       (KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET + (KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT * KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS * sizeof(uint32_t)))

uint32_t kbo_custom_foreign_trade_player_id(uintptr_t trade_ptr, int side, int slot);

typedef struct KboCustomForeignTradePolicyCacheEntry {
    uintptr_t trade_ptr;
    int32_t requested_side;
    uint32_t signature;
    uint32_t org_generation0;
    uint32_t org_generation1;
    int replacement_count;
    DWORD tick;
    int allowed;
    int blocked_side;
    uint32_t team_id;
    uint32_t incoming_player_id;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint8_t valid;
} KboCustomForeignTradePolicyCacheEntry;

enum {
    KBO_CUSTOM_FOREIGN_TRADE_POLICY_CACHE_SIZE = 128,
    KBO_CUSTOM_FOREIGN_TRADE_POLICY_CACHE_TTL_MS = 1000u
};

static KboCustomForeignTradePolicyCacheEntry
    g_kbo_custom_foreign_trade_policy_cache[KBO_CUSTOM_FOREIGN_TRADE_POLICY_CACHE_SIZE];

static uint32_t kbo_custom_foreign_trade_policy_signature(uintptr_t trade_ptr, const uint32_t team_ids[2])
{
    uint32_t h = 2166136261u;
    for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
        h ^= team_ids[side];
        h *= 16777619u;
        for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
            h ^= kbo_custom_foreign_trade_player_id(trade_ptr, side, slot);
            h *= 16777619u;
        }
    }
    return h;
}

static uint32_t kbo_custom_foreign_trade_policy_cache_slot(
    uintptr_t trade_ptr,
    int32_t requested_side,
    uint32_t signature)
{
    uint32_t h = signature ^ (uint32_t)(trade_ptr >> 4) ^ ((uint32_t)requested_side * 2246822519u);
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_TRADE_POLICY_CACHE_SIZE - 1u);
}

static int kbo_custom_foreign_trade_policy_cache_hit(
    uintptr_t trade_ptr,
    int32_t requested_side,
    uint32_t signature,
    const uint32_t team_ids[2],
    int* out_allowed,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit)
{
    uint32_t org_generation0 = team_ids[0] != 0u
        ? kbo_foreign_org_count_cache_generation_for_team(team_ids[0])
        : 0u;
    uint32_t org_generation1 = team_ids[1] != 0u
        ? kbo_foreign_org_count_cache_generation_for_team(team_ids[1])
        : 0u;
    int replacement_count = g_kbo_foreign_injury_replacement_count;
    KboCustomForeignTradePolicyCacheEntry* entry =
        &g_kbo_custom_foreign_trade_policy_cache[
            kbo_custom_foreign_trade_policy_cache_slot(trade_ptr, requested_side, signature)];
    DWORD now = GetTickCount();
    if (!entry->valid
            || entry->trade_ptr != trade_ptr
            || entry->requested_side != requested_side
            || entry->signature != signature
            || entry->org_generation0 != org_generation0
            || entry->org_generation1 != org_generation1
            || entry->replacement_count != replacement_count
            || entry->tick == 0u
            || now - entry->tick > KBO_CUSTOM_FOREIGN_TRADE_POLICY_CACHE_TTL_MS) {
        return 0;
    }
    if (out_allowed != NULL) { *out_allowed = entry->allowed; }
    if (out_blocked_side != NULL) { *out_blocked_side = entry->blocked_side; }
    if (out_team_id != NULL) { *out_team_id = entry->team_id; }
    if (out_incoming_player_id != NULL) { *out_incoming_player_id = entry->incoming_player_id; }
    if (out_effective_before != NULL) { *out_effective_before = entry->effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = entry->effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = entry->effective_limit; }
    return 1;
}

static void kbo_custom_foreign_trade_policy_cache_store(
    uintptr_t trade_ptr,
    int32_t requested_side,
    uint32_t signature,
    const uint32_t team_ids[2],
    int allowed,
    int blocked_side,
    uint32_t team_id,
    uint32_t incoming_player_id,
    uint32_t effective_before,
    uint32_t effective_after,
    uint32_t effective_limit)
{
    KboCustomForeignTradePolicyCacheEntry* entry =
        &g_kbo_custom_foreign_trade_policy_cache[
            kbo_custom_foreign_trade_policy_cache_slot(trade_ptr, requested_side, signature)];
    entry->valid = 0u;
    entry->trade_ptr = trade_ptr;
    entry->requested_side = requested_side;
    entry->signature = signature;
    entry->org_generation0 = team_ids[0] != 0u
        ? kbo_foreign_org_count_cache_generation_for_team(team_ids[0])
        : 0u;
    entry->org_generation1 = team_ids[1] != 0u
        ? kbo_foreign_org_count_cache_generation_for_team(team_ids[1])
        : 0u;
    entry->replacement_count = g_kbo_foreign_injury_replacement_count;
    entry->allowed = allowed;
    entry->blocked_side = blocked_side;
    entry->team_id = team_id;
    entry->incoming_player_id = incoming_player_id;
    entry->effective_before = effective_before;
    entry->effective_after = effective_after;
    entry->effective_limit = effective_limit;
    entry->tick = GetTickCount();
    entry->valid = 1u;
}

uint32_t kbo_custom_foreign_trade_team_id(uintptr_t trade_ptr, int side)
{
    return *(uint32_t*)(trade_ptr + KBO_CUSTOM_FOREIGN_TRADE_TEAM_ID_OFFSET + ((uintptr_t)side * sizeof(uint32_t)));
}

uint32_t kbo_custom_foreign_trade_player_id(uintptr_t trade_ptr, int side, int slot)
{
    uintptr_t offset = KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET
        + (((uintptr_t)side * KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS + (uintptr_t)slot) * sizeof(uint32_t));
    return *(uint32_t*)(trade_ptr + offset);
}

int kbo_custom_foreign_policy_team_in_trade_scope(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id != 0u) {
        return team_league_id == kbo_league_id;
    }
    return team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
}

int kbo_custom_foreign_policy_trade_countable_player(
    uint32_t player_id,
    uint8_t** out_player,
    int* out_asian_quota)
{
    if (out_player != NULL) { *out_player = NULL; }
    if (out_asian_quota != NULL) { *out_asian_quota = 0; }
    if (player_id == 0u) {
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    uint8_t replacement_slot_type = 0u;
    if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
        return 0;
    }

    if (out_player != NULL) { *out_player = player; }
    if (out_asian_quota != NULL) { *out_asian_quota = kbo_player_is_asian_quota_candidate(player); }
    return 1;
}

void kbo_custom_foreign_policy_trade_adjust_counts_for_player(
    uint32_t team_id,
    uint32_t player_id,
    int incoming,
    uint32_t* asian_count,
    uint32_t* non_asian_count,
    uint32_t* incoming_foreign_count,
    uint32_t* first_incoming_player_id)
{
    uint8_t* player = NULL;
    int asian_quota = 0;
    if (!kbo_custom_foreign_policy_trade_countable_player(player_id, &player, &asian_quota)) {
        return;
    }

    int already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(player, team_id);
    if (incoming) {
        if (already_in_org) {
            return;
        }
        if (asian_quota) {
            (*asian_count)++;
        } else {
            (*non_asian_count)++;
        }
        if (incoming_foreign_count != NULL) {
            (*incoming_foreign_count)++;
        }
        if (first_incoming_player_id != NULL && *first_incoming_player_id == 0u) {
            *first_incoming_player_id = player_id;
        }
        return;
    }

    if (!already_in_org) {
        return;
    }
    if (asian_quota) {
        if (*asian_count > 0u) {
            (*asian_count)--;
        }
    } else if (*non_asian_count > 0u) {
        (*non_asian_count)--;
    }
}

static int kbo_custom_foreign_policy_trade_has_new_incoming_foreign(
    uintptr_t trade_ptr,
    int incoming_side,
    uint32_t team_id,
    uint32_t* out_first_incoming_player_id)
{
    if (out_first_incoming_player_id != NULL) { *out_first_incoming_player_id = 0u; }
    for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
        uint32_t player_id = kbo_custom_foreign_trade_player_id(trade_ptr, incoming_side, slot);
        uint8_t* player = NULL;
        int asian_quota = 0;
        if (!kbo_custom_foreign_policy_trade_countable_player(player_id, &player, &asian_quota)
                || kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
            continue;
        }
        (void)asian_quota;
        if (out_first_incoming_player_id != NULL) { *out_first_incoming_player_id = player_id; }
        return 1;
    }
    return 0;
}

uint32_t kbo_custom_foreign_policy_trade_extra_slots(
    uintptr_t trade_ptr,
    int incoming_side,
    uint32_t team_id)
{
    int regular_seen = 0;
    int asian_seen = 0;
    uint32_t extra_slots = 0u;

    for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
        uint32_t player_id = kbo_custom_foreign_trade_player_id(trade_ptr, incoming_side, slot);
        uint8_t* player = NULL;
        int asian_quota = 0;
        if (!kbo_custom_foreign_policy_trade_countable_player(player_id, &player, &asian_quota)
                || kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
            continue;
        }

        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        if (!kbo_custom_foreign_policy_extra_slots_for_candidate(team_id, player, &slot_type, &injured_player_id)) {
            continue;
        }
        (void)injured_player_id;
        if (slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA) {
            if (!asian_seen) {
                asian_seen = 1;
                extra_slots++;
            }
        } else if (slot_type == KBO_FOREIGN_INJURY_SLOT_REGULAR && !regular_seen) {
            regular_seen = 1;
            extra_slots++;
        }
        (void)asian_quota;
    }

    return extra_slots;
}

int kbo_custom_foreign_policy_trade_allows(
    uintptr_t trade_ptr,
    int32_t requested_side,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit)
{
    if (out_blocked_side != NULL) { *out_blocked_side = -1; }
    if (out_team_id != NULL) { *out_team_id = 0u; }
    if (out_incoming_player_id != NULL) { *out_incoming_player_id = 0u; }
    if (out_effective_before != NULL) { *out_effective_before = 0u; }
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT; }
    (void)requested_side;

    if (trade_ptr == 0
            || !memory_range_readable((void*)trade_ptr, KBO_CUSTOM_FOREIGN_TRADE_READABLE_BYTES)) {
        return 1;
    }

    uint32_t team_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT] = {
        kbo_custom_foreign_trade_team_id(trade_ptr, 0),
        kbo_custom_foreign_trade_team_id(trade_ptr, 1)
    };
    uint32_t signature = kbo_custom_foreign_trade_policy_signature(trade_ptr, team_ids);
    int cached_allowed = 1;
    if (kbo_custom_foreign_trade_policy_cache_hit(
            trade_ptr,
            requested_side,
            signature,
            team_ids,
            &cached_allowed,
            out_blocked_side,
            out_team_id,
            out_incoming_player_id,
            out_effective_before,
            out_effective_after,
            out_effective_limit)) {
        return cached_allowed;
    }

    for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
        uint32_t team_id = team_ids[side];
        if (!kbo_custom_foreign_policy_team_in_trade_scope(team_id)) {
            continue;
        }

        int incoming_side = 1 - side;
        uint32_t first_incoming_player_id = 0u;
        if (!kbo_custom_foreign_policy_trade_has_new_incoming_foreign(
                trade_ptr,
                incoming_side,
                team_id,
                &first_incoming_player_id)) {
            continue;
        }

        uint32_t foreign_count = 0u;
        uint32_t asian_count = 0u;
        uint32_t non_asian_count = 0u;
        kbo_count_team_asian_quota_probe_fresh(team_id, &foreign_count, &asian_count, &non_asian_count);
        (void)foreign_count;

        uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
        uint32_t asian_after = asian_count;
        uint32_t non_asian_after = non_asian_count;
        uint32_t incoming_foreign_count = 0u;

        for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
            kbo_custom_foreign_policy_trade_adjust_counts_for_player(
                team_id,
                kbo_custom_foreign_trade_player_id(trade_ptr, side, slot),
                0,
                &asian_after,
                &non_asian_after,
                NULL,
                NULL);
        }
        for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
            kbo_custom_foreign_policy_trade_adjust_counts_for_player(
                team_id,
                kbo_custom_foreign_trade_player_id(trade_ptr, incoming_side, slot),
                1,
                &asian_after,
                &non_asian_after,
                &incoming_foreign_count,
                &first_incoming_player_id);
        }

        uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        if (incoming_foreign_count > 0u && effective_after > effective_limit) {
            effective_limit += kbo_custom_foreign_policy_trade_extra_slots(trade_ptr, incoming_side, team_id);
        }
        if (incoming_foreign_count > 0u && effective_after > effective_limit) {
            if (out_blocked_side != NULL) { *out_blocked_side = side; }
            if (out_team_id != NULL) { *out_team_id = team_id; }
            if (out_incoming_player_id != NULL) { *out_incoming_player_id = first_incoming_player_id; }
            if (out_effective_before != NULL) { *out_effective_before = effective_before; }
            if (out_effective_after != NULL) { *out_effective_after = effective_after; }
            if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
            kbo_custom_foreign_trade_policy_cache_store(
                trade_ptr,
                requested_side,
                signature,
                team_ids,
                0,
                side,
                team_id,
                first_incoming_player_id,
                effective_before,
                effective_after,
                effective_limit);
            return 0;
        }
    }

    kbo_custom_foreign_trade_policy_cache_store(
        trade_ptr,
        requested_side,
        signature,
        team_ids,
        1,
        -1,
        0u,
        0u,
        0u,
        0u,
        KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT);
    return 1;
}

