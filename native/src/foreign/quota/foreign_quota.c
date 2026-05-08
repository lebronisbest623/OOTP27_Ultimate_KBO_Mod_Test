#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../bootstrap/ootp_offsets.h"
#include "../../bootstrap/hook_entrypoints.h"
#include "../../bootstrap/perf_probe.h"
#include "../../core/core_text_date.h"
#include "../../core/core_league_context_parts/league_context_lookup.h"
#include "../../core/core_flags/flags_api.h"
#include "../../core/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/team_lookup.h"
#include "../../team/team_org_assignment_query.h"
#include "../foreign_waiver_date.h"
#include "../foreign_waiver_player_eval.h"
#include "../foreign_waiver_policy.h"
#include "../injury/foreign_injury.h"
#include "../replacement_seed/foreign_replacement_seed.h"
/* ---- native/src/foreign/quota/foreign_asian_quota_counts.inc ---- */
/* Asian quota organization and active-roster count helpers. Included from native/KBOFix.c. */

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
            if (slot <= 20) {
                append_logf(
                    "foreign replacement player seed excluded from org count team=%u player=%u key_slot=%s nation=%u",
                    team_id,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    replacement_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular",
                    nation_id);
            } else if (slot == 21) {
                append_log_line("foreign replacement player seed org-count exclusion log suppressed after 20 entries");
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

static void kbo_count_team_asian_quota_probe_fresh(
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
}

uint32_t kbo_effective_foreign_count_with_asian_quota(uint32_t asian_count, uint32_t non_asian_foreign_count)
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


/* ---- native/src/foreign/quota/foreign_custom_foreign_signing_policy_parts/pending_offers.inc ---- */
/* Custom foreign-player signing policy. Included from native/KBOFix.c. */

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

void kbo_record_custom_foreign_pending_offer(uint32_t team_id, uint8_t* candidate, uint32_t today)
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

/* ---- native/src/foreign/quota/foreign_custom_foreign_signing_policy_parts/candidate_limits.inc ---- */
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
    return effective_after <= effective_limit;
}

/* ---- native/src/foreign/quota/foreign_custom_foreign_signing_policy_parts/trade_policy.inc ---- */
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

/* ---- native/src/foreign/quota/foreign_active_count_policy.inc ---- */
/* Active foreign count adjustment policy. Included from native/KBOFix.c. */

static int32_t kbo_apply_active_asian_quota_count_exception(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count)
{
    if (original_count <= 0) {
        return original_count;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    kbo_count_active_asian_quota_by_position(team_ptr, &asian_hitters, &asian_pitchers);

    int use_exception = 0;
    if (pitcher_count) {
        use_exception = (asian_pitchers > 0u && asian_hitters == 0u);
    } else {
        use_exception = (asian_hitters > 0u);
    }
    if (!use_exception) {
        return original_count;
    }

    int32_t adjusted = original_count - 1;
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t team_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        append_logf(
            "asian quota active foreign slot exception team=%u type=%s original=%d adjusted=%d asian_hitters=%u asian_pitchers=%u",
            team_id,
            pitcher_count ? "pitcher" : "hitter",
            original_count,
            adjusted,
            asian_hitters,
            asian_pitchers);
    }
    return adjusted;
}

static int32_t kbo_custom_foreign_policy_neutralized_count(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count)
{
    if (original_count <= 0) {
        return original_count;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    uint32_t effective = kbo_effective_foreign_count_with_asian_quota(
        asian_hitters + asian_pitchers,
        non_asian_hitters + non_asian_pitchers);

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t team_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        append_logf(
            "custom foreign policy neutralized OOTP active count team=%u type=%s original=%d adjusted=0 active_effective=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            pitcher_count ? "pitcher" : "hitter",
            original_count,
            effective,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return 0;
}


/* ---- native/src/foreign/quota/foreign_active_count_wrappers.inc ---- */
/* OOTP active foreign count wrappers. Included from native/KBOFix.c. */

__declspec(noinline) int32_t ootp_kbo_active_foreign_hitter_count_wrapper(
    uintptr_t team_ptr,
    uintptr_t original_func_ptr)
{
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    typedef int32_t (__fastcall *OriginalFn)(uintptr_t);
    int32_t original = ((OriginalFn)original_func_ptr)(team_ptr);
    int32_t result = original;
    if (kbo_custom_foreign_policy_enabled()) {
        result = kbo_custom_foreign_policy_neutralized_count(team_ptr, original, 0);
    } else {
        result = kbo_apply_active_asian_quota_count_exception(team_ptr, original, 0);
    }
    kbo_perf_probe_record(
        "active_foreign_hitter_count",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
    return result;
}

__declspec(noinline) int32_t ootp_kbo_active_foreign_pitcher_count_wrapper(
    uintptr_t team_ptr,
    uintptr_t original_func_ptr)
{
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    typedef int32_t (__fastcall *OriginalFn)(uintptr_t);
    int32_t original = ((OriginalFn)original_func_ptr)(team_ptr);
    int32_t result = original;
    if (kbo_custom_foreign_policy_enabled()) {
        result = kbo_custom_foreign_policy_neutralized_count(team_ptr, original, 1);
    } else {
        result = kbo_apply_active_asian_quota_count_exception(team_ptr, original, 1);
    }
    kbo_perf_probe_record(
        "active_foreign_pitcher_count",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
    return result;
}


/* ---- native/src/foreign/quota/foreign_callup_policy.inc ---- */
/* Foreign-player callup limit policy and wrappers. Included from native/KBOFix.c. */

static uint8_t kbo_custom_foreign_policy_callup_allows(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t ootp_limit,
    int check_type)
{
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return 0u;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0u;
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return 1u;
    }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    int candidate_pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
    if (check_type == 1 && candidate_pitcher) {
        return 1u;
    }
    if (check_type == 2 && !candidate_pitcher) {
        return 1u;
    }

    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_id == 0u || player_id == 0u) {
        return 0u;
    }
    if (kbo_team_active_roster_contains_player(team_ptr, player_id)) {
        return 1u;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    uint32_t asian_before = asian_hitters + asian_pitchers;
    uint32_t non_asian_before = non_asian_hitters + non_asian_pitchers;
    uint32_t asian_after = asian_before;
    uint32_t non_asian_after = non_asian_before;
    if (kbo_player_is_asian_quota_candidate(player)) {
        asian_after++;
    } else {
        non_asian_after++;
    }

    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
        team_id,
        player,
        &slot_type,
        &injured_player_id);
    uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_before, non_asian_before);
    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT + extra_slots;
    uint8_t allowed = effective_after <= effective_limit ? 1u : 0u;

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 240) {
        append_logf(
            "custom foreign policy callup team=%u player=%u type=%s active_count=%d ootp_limit=%d allowed=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            player_id,
            check_type == 1 ? "hitter" : (check_type == 2 ? "pitcher" : "total"),
            active_count,
            ootp_limit,
            (uint32_t)allowed,
            effective_before,
            effective_after,
            effective_limit,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return allowed;
}

static uint8_t kbo_callup_foreign_limit_allows_with_asian_quota(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t limit,
    int check_type)
{
    if (kbo_custom_foreign_policy_enabled()) {
        return kbo_custom_foreign_policy_callup_allows(team_ptr, player_ptr, active_count, limit, check_type);
    }
    if (active_count < limit) {
        return 1u;
    }
    if (limit <= 0 || player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return 0u;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t injury_effective_after = 0u;
    uint8_t injury_slot_type = 0u;
    if (kbo_foreign_injury_replacement_callup_exception_available(
            team_ptr,
            player,
            limit,
            check_type,
            &injury_effective_after,
            &injury_slot_type)) {
        static volatile LONG injury_log_count = 0;
        LONG injury_slot = InterlockedIncrement(&injury_log_count);
        if (injury_slot <= 160) {
            uint32_t team_id = 0u;
            if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
                team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
            }
            append_logf(
                "foreign injury replacement callup exception team=%u player=%u type=%s active_count=%d limit=%d effective_after=%u",
                team_id,
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                kbo_foreign_injury_slot_label(injury_slot_type),
                active_count,
                limit,
                injury_effective_after);
        }
        return 1u;
    }
    if (!kbo_nation_is_asian_quota_candidate(nation_id)) {
        return 0u;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    int candidate_pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
    uint32_t effective_after = 0u;
    if (check_type == 1) {
        uint32_t asian_after = asian_hitters + (candidate_pitcher ? 0u : 1u);
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_hitters);
    } else if (check_type == 2) {
        uint32_t asian_after = asian_pitchers + (candidate_pitcher ? 1u : 0u);
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_pitchers);
    } else {
        uint32_t asian_after = asian_hitters + asian_pitchers + 1u;
        uint32_t non_asian_after = non_asian_hitters + non_asian_pitchers;
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    }

    uint8_t allowed = (effective_after <= (uint32_t)limit) ? 1u : 0u;
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t team_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        append_logf(
            "asian quota callup limit check team=%u player=%u nation=%u type=%s active_count=%d limit=%d effective_after=%u allowed=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
            nation_id,
            check_type == 1 ? "hitter" : (check_type == 2 ? "pitcher" : "total"),
            active_count,
            limit,
            effective_after,
            (uint32_t)allowed,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return allowed;
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_hitter_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    return kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 1);
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_pitcher_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    return kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 2);
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_total_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    return kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 3);
}


/* ---- native/src/foreign/quota/foreign_asian_quota_probe_log.inc ---- */
/* Asian quota signability and offer probe logging. Included from native/KBOFix.c. */

static int kbo_asian_quota_exception_available_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_team_foreign,
    uint32_t* out_team_asian,
    uint32_t* out_team_non_asian,
    uint32_t* out_team_effective,
    uint32_t* out_effective_after)
{
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);

    uint32_t effective = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    int candidate_is_asian = kbo_player_is_asian_quota_candidate(candidate);
    int exception_available = candidate_is_asian && asian_count == 0u;
    uint32_t effective_after = effective;
    if (!exception_available) {
        effective_after++;
    }

    if (out_team_foreign != NULL) { *out_team_foreign = foreign_count; }
    if (out_team_asian != NULL) { *out_team_asian = asian_count; }
    if (out_team_non_asian != NULL) { *out_team_non_asian = non_asian_count; }
    if (out_team_effective != NULL) { *out_team_effective = effective; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    return exception_available;
}

void kbo_log_asian_quota_signability_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    int original_signability,
    uintptr_t caller_rva)
{
    static LONG log_enabled_initialized = 0;
    static LONG log_enabled = 0;
    if (InterlockedCompareExchange(&log_enabled_initialized, 1, 0) == 0) {
        InterlockedExchange(
            &log_enabled,
            read_kbo_localappdata_flag_file("enable_kbo_asian_quota_probe_logs.txt") ? 1 : 0);
    }
    if (InterlockedCompareExchange(&log_enabled, 0, 0) == 0) {
        return;
    }

    if (player == NULL || player_id == 0u || team_id <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 240) {
        return;
    }

    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    uint32_t team_effective = 0u;
    uint32_t effective_after = 0u;
    int exception_available = kbo_asian_quota_exception_available_for_candidate(
        (uint32_t)team_id,
        player,
        &team_foreign,
        &team_asian,
        &team_non_asian,
        &team_effective,
        &effective_after);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);

    append_logf(
        "asian quota signability probe player=%u requester_team=%d nation=%u asian_candidate=%d exception_available=%d"
        " original=%d team_foreign_raw=%u team_asian=%u team_non_asian=%u team_effective_foreign=%u effective_after=%u current_team=%u active_team=%u caller_rva=0x%llx",
        player_id,
        team_id,
        nation_id,
        kbo_player_is_asian_quota_candidate(player),
        exception_available,
        original_signability,
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective,
        effective_after,
        current_team_id,
        active_team_id,
        (unsigned long long)caller_rva);
}

void kbo_log_asian_quota_offer_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    uint8_t original_result,
    int32_t flag)
{
    static LONG log_enabled_initialized = 0;
    static LONG log_enabled = 0;
    if (InterlockedCompareExchange(&log_enabled_initialized, 1, 0) == 0) {
        InterlockedExchange(
            &log_enabled,
            read_kbo_localappdata_flag_file("enable_kbo_asian_quota_probe_logs.txt") ? 1 : 0);
    }
    if (InterlockedCompareExchange(&log_enabled, 0, 0) == 0) {
        return;
    }

    if (player == NULL || player_id == 0u || team_id <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 160) {
        return;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    uint32_t effective_count = 0u;
    uint32_t effective_after = 0u;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    int exception_available = kbo_asian_quota_exception_available_for_candidate(
        (uint32_t)team_id,
        player,
        &foreign_count,
        &asian_count,
        &non_asian_count,
        &effective_count,
        &effective_after);

    append_logf(
        "asian quota offer probe player=%u requester_team=%d nation=%u asian_candidate=%d original=%u flag=%d"
        " exception_available=%d team_foreign_raw=%u team_asian=%u team_non_asian_foreign=%u team_effective_foreign=%u effective_after=%u current_team=%u active_team=%u current_league=%u today=%u",
        player_id,
        team_id,
        nation_id,
        kbo_nation_is_asian_quota_candidate(nation_id),
        (uint32_t)original_result,
        flag,
        exception_available,
        foreign_count,
        asian_count,
        non_asian_count,
        effective_count,
        effective_after,
        current_team_id,
        active_team_id,
        current_league_id,
        today);
}


