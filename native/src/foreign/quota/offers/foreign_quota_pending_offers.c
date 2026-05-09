#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/profiling/perf_probe.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../injury/api/foreign_injury.h"
#include "../../replacement_seed/api/foreign_replacement_seed.h"
#include "../counts/foreign_quota_counts.h"

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

KboCustomForeignPendingOffer g_kbo_custom_foreign_pending_offers[KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX];
LONG g_kbo_custom_foreign_pending_offer_lock = 0;
int g_kbo_custom_foreign_pending_offer_count = 0;

void kbo_custom_foreign_pending_offer_lock(void)
{
    while (InterlockedCompareExchange(&g_kbo_custom_foreign_pending_offer_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_custom_foreign_pending_offer_unlock(void)
{
    InterlockedExchange(&g_kbo_custom_foreign_pending_offer_lock, 0);
}

int kbo_custom_foreign_pending_offer_is_stale(uint32_t offer_date, uint32_t today)
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

int kbo_custom_foreign_pending_offer_player_now_in_org(uint32_t team_id, uint32_t player_id)
{
    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    return player != NULL
        && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
        && kbo_player_current_assignment_matches_team_or_affiliate(player, team_id);
}

void kbo_custom_foreign_count_pending_offers(
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

