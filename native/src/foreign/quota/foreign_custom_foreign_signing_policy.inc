/* Custom foreign-player signing policy. Included from foreign_asian_quota.inc. */

typedef struct KboCustomForeignPendingOffer {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t date_yyyymmdd;
    uint8_t asian_quota_candidate;
} KboCustomForeignPendingOffer;

enum { KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX = 256 };

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
    return offer_serial == 0u || today_serial == 0u || offer_serial > today_serial || today_serial - offer_serial > 120u;
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

