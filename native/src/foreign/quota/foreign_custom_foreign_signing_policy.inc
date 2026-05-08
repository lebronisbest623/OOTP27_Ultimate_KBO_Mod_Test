/* Custom foreign-player signing policy. Included from foreign_asian_quota.inc. */

typedef struct KboCustomForeignPendingOffer {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t date_yyyymmdd;
    uint8_t asian_quota_candidate;
} KboCustomForeignPendingOffer;

enum {
    KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX = 1024,
    KBO_CUSTOM_FOREIGN_PENDING_OFFER_TTL_DAYS = 45
};

static KboCustomForeignPendingOffer g_kbo_custom_foreign_pending_offers[KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX];
static LONG g_kbo_custom_foreign_pending_offer_lock = 0;
static int g_kbo_custom_foreign_pending_offer_count = 0;

static void kbo_custom_foreign_pending_offer_lock(void)
{
    while (InterlockedCompareExchange(&g_kbo_custom_foreign_pending_offer_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_custom_foreign_pending_offer_unlock(void)
{
    InterlockedExchange(&g_kbo_custom_foreign_pending_offer_lock, 0);
}

static int kbo_custom_foreign_pending_offer_is_stale(uint32_t offer_date, uint32_t today)
{
    if (offer_date == 0u || today == 0u) {
        return 0;
    }
    uint32_t offer_serial = kbo_date_serial(offer_date / 10000u, (offer_date / 100u) % 100u, offer_date % 100u);
    uint32_t today_serial = kbo_date_serial(today / 10000u, (today / 100u) % 100u, today % 100u);
    return offer_serial == 0u
        || today_serial == 0u
        || offer_serial > today_serial
        || today_serial - offer_serial > KBO_CUSTOM_FOREIGN_PENDING_OFFER_TTL_DAYS;
}

static int kbo_custom_foreign_pending_offer_player_now_in_org(uint32_t team_id, uint32_t player_id)
{
    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    return player != NULL
        && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
        && kbo_player_current_assignment_matches_team_or_affiliate(player, team_id);
}

static void kbo_custom_foreign_count_pending_offers(
    uint32_t team_id,
    uint32_t today,
    uint32_t candidate_id,
    uint32_t* out_asian_pending,
    uint32_t* out_non_asian_pending,
    int* out_candidate_pending)
{
    if (out_asian_pending != NULL) { *out_asian_pending = 0u; }
    if (out_non_asian_pending != NULL) { *out_non_asian_pending = 0u; }
    if (out_candidate_pending != NULL) { *out_candidate_pending = 0; }
    if (team_id == 0u) {
        return;
    }

    kbo_custom_foreign_pending_offer_lock();

    int write_index = 0;
    uint32_t asian_pending = 0u;
    uint32_t non_asian_pending = 0u;
    int candidate_pending = 0;
    for (int i = 0; i < g_kbo_custom_foreign_pending_offer_count; i++) {
        KboCustomForeignPendingOffer rec = g_kbo_custom_foreign_pending_offers[i];
        if (rec.team_id == 0u || rec.player_id == 0u
                || kbo_custom_foreign_pending_offer_is_stale(rec.date_yyyymmdd, today)
                || kbo_custom_foreign_pending_offer_player_now_in_org(rec.team_id, rec.player_id)) {
            continue;
        }

        g_kbo_custom_foreign_pending_offers[write_index++] = rec;
        if (rec.team_id != team_id) {
            continue;
        }
        if (rec.player_id == candidate_id) {
            candidate_pending = 1;
        }
        if (rec.asian_quota_candidate) {
            asian_pending++;
        } else {
            non_asian_pending++;
        }
    }
    g_kbo_custom_foreign_pending_offer_count = write_index;

    kbo_custom_foreign_pending_offer_unlock();

    if (out_asian_pending != NULL) { *out_asian_pending = asian_pending; }
    if (out_non_asian_pending != NULL) { *out_non_asian_pending = non_asian_pending; }
    if (out_candidate_pending != NULL) { *out_candidate_pending = candidate_pending; }
}

static void kbo_record_custom_foreign_pending_offer(uint32_t team_id, uint8_t* candidate, uint32_t today)
{
    if (team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)
            || kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id)) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return;
    }

    uint8_t asian = kbo_player_is_asian_quota_candidate(candidate) ? 1u : 0u;
    kbo_custom_foreign_pending_offer_lock();

    int empty_slot = -1;
    for (int i = 0; i < g_kbo_custom_foreign_pending_offer_count; i++) {
        KboCustomForeignPendingOffer* rec = &g_kbo_custom_foreign_pending_offers[i];
        if (rec->team_id == team_id && rec->player_id == player_id) {
            rec->date_yyyymmdd = today;
            rec->asian_quota_candidate = asian;
            kbo_custom_foreign_pending_offer_unlock();
            return;
        }
    }
    if (g_kbo_custom_foreign_pending_offer_count < KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX) {
        empty_slot = g_kbo_custom_foreign_pending_offer_count++;
    }
    if (empty_slot >= 0) {
        g_kbo_custom_foreign_pending_offers[empty_slot] = (KboCustomForeignPendingOffer){
            .team_id = team_id,
            .player_id = player_id,
            .date_yyyymmdd = today,
            .asian_quota_candidate = asian
        };
    }

    kbo_custom_foreign_pending_offer_unlock();

    static volatile LONG pending_offer_log_count = 0;
    LONG slot = InterlockedIncrement(&pending_offer_log_count);
    if (slot <= 200) {
        append_logf(
            "custom foreign policy pending offer recorded team=%u player=%u asian=%u today=%u",
            team_id,
            player_id,
            (uint32_t)asian,
            today);
    }
}

static uint32_t kbo_custom_foreign_policy_extra_slots_for_candidate(
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

static int kbo_custom_foreign_policy_can_override_original_block(uint8_t* candidate, uint32_t team_id)
{
    if (candidate == NULL || team_id == 0u || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(candidate + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    return current_team_id == 0u || kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
}

static int kbo_custom_foreign_policy_team_allows_candidate(
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
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    (void)foreign_count;

    uint32_t today = 0u;
    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    uint32_t pending_asian_count = 0u;
    uint32_t pending_non_asian_count = 0u;
    int candidate_pending = 0;
    kbo_custom_foreign_count_pending_offers(
        team_id,
        today,
        candidate_id,
        &pending_asian_count,
        &pending_non_asian_count,
        &candidate_pending);
    asian_count += pending_asian_count;
    non_asian_count += pending_non_asian_count;

    uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    uint32_t asian_after = asian_count;
    uint32_t non_asian_after = non_asian_count;
    int already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
    if (!already_in_org && !candidate_pending) {
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
    return effective_after <= effective_limit;
}

static int kbo_custom_foreign_policy_team_allows_final_signing(
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
    return effective_after <= effective_limit;
}

#define KBO_CUSTOM_FOREIGN_TRADE_TEAM_ID_OFFSET       0x08u
#define KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET     0x10u
#define KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT           2
#define KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS         10
#define KBO_CUSTOM_FOREIGN_TRADE_READABLE_BYTES       (KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET + (KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT * KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS * sizeof(uint32_t)))

static uint32_t kbo_custom_foreign_trade_team_id(uintptr_t trade_ptr, int side)
{
    return *(uint32_t*)(trade_ptr + KBO_CUSTOM_FOREIGN_TRADE_TEAM_ID_OFFSET + ((uintptr_t)side * sizeof(uint32_t)));
}

static uint32_t kbo_custom_foreign_trade_player_id(uintptr_t trade_ptr, int side, int slot)
{
    uintptr_t offset = KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET
        + (((uintptr_t)side * KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS + (uintptr_t)slot) * sizeof(uint32_t));
    return *(uint32_t*)(trade_ptr + offset);
}

static int kbo_custom_foreign_policy_team_in_trade_scope(uint32_t team_id)
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

static int kbo_custom_foreign_policy_trade_countable_player(
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

static void kbo_custom_foreign_policy_trade_adjust_counts_for_player(
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

static uint32_t kbo_custom_foreign_policy_trade_extra_slots(
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

static int kbo_custom_foreign_policy_trade_allows(
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

    for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
        uint32_t team_id = team_ids[side];
        if (!kbo_custom_foreign_policy_team_in_trade_scope(team_id)) {
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
        uint32_t first_incoming_player_id = 0u;
        int incoming_side = 1 - side;

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
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT
            + kbo_custom_foreign_policy_trade_extra_slots(trade_ptr, incoming_side, team_id);
        if (incoming_foreign_count > 0u && effective_after > effective_limit) {
            if (out_blocked_side != NULL) { *out_blocked_side = side; }
            if (out_team_id != NULL) { *out_team_id = team_id; }
            if (out_incoming_player_id != NULL) { *out_incoming_player_id = first_incoming_player_id; }
            if (out_effective_before != NULL) { *out_effective_before = effective_before; }
            if (out_effective_after != NULL) { *out_effective_after = effective_after; }
            if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
            return 0;
        }
    }

    return 1;
}
