#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../core/core_flags/api/flags_api.h"
#include "../../../../../core/logging/core_log.h"
#include "../../../../../runtime_memory/runtime_memory.h"
#include "../../../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../../../../team/lookup/team_lookup.h"
#include "../../../../common/dates/foreign_waiver_date.h"
#include "../../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../controller/foreign_ai_controller.h"
#include "../../../../rights/query/foreign_waiver_rights_query.h"
#include "../candidate_array/foreign_signability_ai_fa_candidate_array.h"
#include "foreign_signability_foreign_ai_fa_retained_candidates.h"

#define KBO_AI_FA_STATUS_FORCED_RETENTION_MAX 16

typedef struct KboAiFaStatusRetainedCandidate {
    uintptr_t player_ptr;
    uint32_t player_id, current_team_id, active_team_id, original_team_id;
    uint32_t default_team_id, loan_team_id, draft_league_id;
    int32_t score, threshold;
    uint8_t position_group, position_role, asian, contract_level;
    int already_in_org, market_free_agent, holder_org_candidate;
    const char* reject_reason;
} KboAiFaStatusRetainedCandidate;

static int kbo_ai_fa_status_retention_recently_attempted(
    uintptr_t frame_ptr,
    uintptr_t candidate_array,
    uint32_t requester_team_id,
    uint32_t today)
{
    static uintptr_t last_frame_ptr = 0;
    static uintptr_t last_candidate_array = 0;
    static uint32_t last_requester_team_id = 0u;
    static uint32_t last_today = 0u;
    static DWORD last_attempt_tick = 0u;

    DWORD now = GetTickCount();
    if (last_frame_ptr == frame_ptr
            && last_candidate_array == candidate_array
            && last_requester_team_id == requester_team_id
            && last_today == today
            && last_attempt_tick != 0u
            && now - last_attempt_tick < 1000u) {
        return 1;
    }

    last_frame_ptr = frame_ptr;
    last_candidate_array = candidate_array;
    last_requester_team_id = requester_team_id;
    last_today = today;
    last_attempt_tick = now;
    return 0;
}

static int kbo_ai_fa_status_retention_priority_enabled(void)
{
    return read_kbo_localappdata_flag_file("enable_foreign_ai_roster_management.txt")
        || kbo_foreign_ai_controller_enabled();
}

static uint32_t kbo_ai_fa_status_player_u32(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(player + offset);
}

static uint8_t kbo_ai_fa_status_player_u8(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint8_t))) {
        return 0u;
    }
    return player[offset];
}

static void kbo_ai_fa_status_refresh_candidate_assignment(
    uint8_t* player,
    uint32_t requester_team_id,
    KboAiFaStatusRetainedCandidate* candidate)
{
    if (player == NULL || candidate == NULL) {
        return;
    }
    candidate->current_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    candidate->active_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    candidate->original_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    candidate->default_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    candidate->loan_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    candidate->draft_league_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    candidate->contract_level = kbo_ai_fa_status_player_u8(player, OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    candidate->already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(player, requester_team_id);
    candidate->market_free_agent =
        candidate->current_team_id == 0u
        && candidate->loan_team_id == 0u
        && candidate->draft_league_id == 0u;
    candidate->holder_org_candidate =
        candidate->loan_team_id == 0u
        && (candidate->already_in_org
            || (candidate->current_team_id == 0u
                && candidate->active_team_id == requester_team_id));
}

static int kbo_ai_fa_status_restore_rights_only_assignment(
    uint8_t* player,
    uint32_t requester_team_id,
    uint32_t today,
    KboAiFaStatusRetainedCandidate* candidate)
{
    if (player == NULL || candidate == NULL
            || requester_team_id == 0u
            || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || candidate->contract_level == 1u
            || candidate->current_team_id != requester_team_id
            || candidate->active_team_id != requester_team_id
            || !kbo_has_active_foreign_waiver_right(requester_team_id, candidate->player_id, today)) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(requester_team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (league_id == 0u) {
        return 0;
    }

    uint32_t before_current = candidate->current_team_id;
    uint32_t before_active = candidate->active_team_id;
    uint32_t before_original = candidate->original_team_id;
    uint32_t before_default = candidate->default_team_id;
    uint32_t before_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t before_draft = candidate->draft_league_id;
    uint8_t before_level = candidate->contract_level;
    uint8_t before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t before_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t before_dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];

    int removed_roster = kbo_remove_player_id_from_known_team_roster_arrays(team, candidate->player_id);
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = 0u;
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == requester_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) == league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = requester_team_id;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = league_id;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            && before_default != 0u
            && before_default != requester_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = 0u;
    }
    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    int added_restricted = kbo_add_player_id_to_team_fixed_array(
        team,
        OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
        candidate->player_id);
    kbo_ai_fa_status_refresh_candidate_assignment(player, requester_team_id, candidate);

    static volatile LONG repair_log_count = 0;
    LONG slot = InterlockedIncrement(&repair_log_count);
    if (slot <= 200) {
        append_logf(
            "foreign retention priority: repaired rights-only assignment team=%u player=%u today=%u before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_draft=%u before_level=%u before_restricted=%u before_secondary=%u before_dfa=%u after_current=%u after_active=%u after_original=%u after_default=%u after_league=%u after_draft=%u after_level=%u after_restricted=%u after_secondary=%u after_dfa=%u removed_roster=%d added_restricted=%d",
            requester_team_id,
            candidate->player_id,
            today,
            before_current,
            before_active,
            before_original,
            before_default,
            before_league,
            before_draft,
            (uint32_t)before_level,
            (uint32_t)before_restricted,
            (uint32_t)before_secondary,
            (uint32_t)before_dfa,
            candidate->current_team_id,
            candidate->active_team_id,
            candidate->original_team_id,
            candidate->default_team_id,
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            candidate->draft_league_id,
            (uint32_t)candidate->contract_level,
            (uint32_t)player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET],
            (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
            (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
            removed_roster,
            added_restricted);
    }
    return 1;
}

static void kbo_ai_fa_status_log_retained_candidate_eval(
    uint32_t requester_team_id, uint32_t today, const KboAiFaStatusRetainedCandidate* candidate, int can_enter)
{
    static volatile LONG eval_log_count = 0;
    LONG slot = InterlockedIncrement(&eval_log_count);
    if (slot > 600 || candidate == NULL) {
        return;
    }

    append_logf(
        "foreign retention priority: candidate_eval team=%u player=%u can_enter=%d reason=%s score=%d threshold=%d pos=%u/%u asian=%u in_org=%d market=%d holder_org=%d current=%u active=%u original=%u default=%u loan=%u draft=%u level=%u today=%u",
        requester_team_id,
        candidate->player_id,
        can_enter,
        candidate->reject_reason != NULL ? candidate->reject_reason : "ok",
        candidate->score,
        candidate->threshold,
        (uint32_t)candidate->position_group,
        (uint32_t)candidate->position_role,
        (uint32_t)candidate->asian,
        candidate->already_in_org,
        candidate->market_free_agent,
        candidate->holder_org_candidate,
        candidate->current_team_id,
        candidate->active_team_id,
        candidate->original_team_id,
        candidate->default_team_id,
        candidate->loan_team_id,
        candidate->draft_league_id,
        (uint32_t)candidate->contract_level,
        today);
}

static int kbo_ai_fa_status_evaluate_retained_market_candidate(
    uint8_t* player, uint32_t expected_player_id, uint32_t requester_team_id,
    uint32_t today, KboAiFaStatusRetainedCandidate* out_candidate)
{
    KboAiFaStatusRetainedCandidate candidate = {0};
    candidate.player_id = expected_player_id;
    candidate.reject_reason = "ok";
    if (out_candidate != NULL) {
        *out_candidate = candidate;
    }

    if (player == NULL
            || requester_team_id == 0u
            || today == 0u) {
        candidate.reject_reason = "bad_args";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        candidate.reject_reason = "unreadable";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }

    candidate.player_ptr = (uintptr_t)player;
    candidate.player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    candidate.current_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    candidate.active_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    candidate.original_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    candidate.default_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    candidate.loan_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    candidate.draft_league_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    candidate.position_group = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
    candidate.position_role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
    candidate.asian = kbo_player_is_asian_quota_candidate(player) ? 1u : 0u;
    candidate.contract_level = kbo_ai_fa_status_player_u8(player, OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    kbo_ai_fa_status_refresh_candidate_assignment(player, requester_team_id, &candidate);

    if (candidate.player_id != expected_player_id) {
        candidate.reject_reason = "id_mismatch";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        candidate.reject_reason = "retired";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        candidate.reject_reason = "not_foreign";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    if (!kbo_has_active_foreign_waiver_right(requester_team_id, expected_player_id, today)) {
        candidate.reject_reason = "no_active_right";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    kbo_ai_fa_status_restore_rights_only_assignment(
        player,
        requester_team_id,
        today,
        &candidate);
    if (!candidate.market_free_agent && !candidate.holder_org_candidate) {
        candidate.reject_reason = "not_market_free_agent";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }

    candidate.score = kbo_foreign_waiver_value_score(player);
    candidate.threshold = kbo_get_foreign_waiver_value_threshold_for_player(player);
    if (candidate.score < candidate.threshold) {
        candidate.reject_reason = "below_threshold";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }

    if (out_candidate != NULL) { *out_candidate = candidate; }
    return 1;
}

static void kbo_ai_fa_status_sort_retained_candidates(KboAiFaStatusRetainedCandidate* candidates, int count)
{
    if (candidates == NULL || count <= 1) {
        return;
    }

    for (int i = 0; i < count - 1; i++) {
        int best = i;
        for (int j = i + 1; j < count; j++) {
            if (candidates[j].score > candidates[best].score) {
                best = j;
            }
        }
        if (best != i) {
            KboAiFaStatusRetainedCandidate tmp = candidates[i];
            candidates[i] = candidates[best];
            candidates[best] = tmp;
        }
    }
}

int32_t kbo_ai_fa_status_force_retained_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index)
{
    if (candidate_array == 0
            || insert_index < 0
            || requester_team_id == 0u
            || !kbo_ai_fa_status_retention_priority_enabled()) {
        return insert_index;
    }
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return insert_index;
    }

    if (kbo_ai_fa_status_retention_recently_attempted(
            frame_ptr,
            candidate_array,
            requester_team_id,
            today)) {
        return insert_index;
    }

    kbo_prune_expired_foreign_waiver_rights(today);
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();

    uint32_t player_ids[KBO_AI_FA_STATUS_FORCED_RETENTION_MAX] = {0};
    int player_count = 0;
    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count && player_count < KBO_AI_FA_STATUS_FORCED_RETENTION_MAX; i++) {
        const KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == requester_team_id && kbo_is_foreign_waiver_right_active(rec, today)) {
            player_ids[player_count++] = rec->player_id;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    if (player_count == 0) {
        return insert_index;
    }

    KboAiFaStatusRetainedCandidate candidates[KBO_AI_FA_STATUS_FORCED_RETENTION_MAX] = {0};
    int candidate_count = 0;
    int considered = 0;
    int protectable = 0;
    int already_present = 0;
    int rejected = 0;
    for (int i = 0; i < player_count; i++) {
        uint32_t retained_player_id = player_ids[i];
        uint8_t* retained_player = kbo_find_player_by_id(retained_player_id, NULL, NULL);
        KboAiFaStatusRetainedCandidate candidate = {0};
        considered++;
        if (!kbo_ai_fa_status_evaluate_retained_market_candidate(
                retained_player,
                retained_player_id,
                requester_team_id,
                today,
                &candidate)) {
            rejected++;
            kbo_ai_fa_status_log_retained_candidate_eval(
                requester_team_id,
                today,
                &candidate,
                0);
            continue;
        }
        kbo_ai_fa_status_log_retained_candidate_eval(
            requester_team_id,
            today,
            &candidate,
            1);
        protectable++;
        if (candidate_count < KBO_AI_FA_STATUS_FORCED_RETENTION_MAX) {
            candidates[candidate_count++] = candidate;
        }
    }

    kbo_ai_fa_status_sort_retained_candidates(candidates, candidate_count);

    static volatile LONG retention_team_log_count = 0;
    LONG team_slot = InterlockedIncrement(&retention_team_log_count);
    if (team_slot <= 400) {
        append_logf(
            "foreign retention priority: team_probe team=%u today=%u rights=%d considered=%d protectable=%d rejected=%d index=%d candidate_array=%p top_player=%u top_score=%d",
            requester_team_id,
            today,
            player_count,
            considered,
            protectable,
            rejected,
            insert_index,
            (void*)candidate_array,
            candidate_count > 0 ? candidates[0].player_id : 0u,
            candidate_count > 0 ? candidates[0].score : 0);
    }

    for (int i = 0; i < candidate_count; i++) {
        KboAiFaStatusRetainedCandidate* candidate = &candidates[i];
        if (kbo_ai_fa_status_candidate_array_contains(candidate_array, insert_index, candidate->player_ptr)) {
            already_present++;
            continue;
        }

        int32_t before_index = insert_index;
        insert_index = kbo_ai_fa_status_insert_candidate_ptr(
            frame_ptr,
            candidate_array,
            insert_index,
            candidate->player_ptr);
        if (insert_index != before_index) {
            static volatile LONG retention_force_log_count = 0;
            LONG slot = InterlockedIncrement(&retention_force_log_count);
            if (slot <= 200) {
                append_logf(
                    "foreign retention priority: force_retained_candidate team=%u player=%u score=%d threshold=%d pos=%u/%u asian=%u in_org=%d market=%d holder_org=%d current=%u active=%u original=%u default=%u loan=%u draft=%u level=%u index=%d next=%d today=%u",
                    requester_team_id,
                    candidate->player_id,
                    candidate->score,
                    candidate->threshold,
                    (uint32_t)candidate->position_group,
                    (uint32_t)candidate->position_role,
                    (uint32_t)candidate->asian,
                    candidate->already_in_org,
                    candidate->market_free_agent,
                    candidate->holder_org_candidate,
                    candidate->current_team_id,
                    candidate->active_team_id,
                    candidate->original_team_id,
                    candidate->default_team_id,
                    candidate->loan_team_id,
                    candidate->draft_league_id,
                    (uint32_t)candidate->contract_level,
                    before_index,
                    insert_index,
                    today);
            }
        } else {
            static volatile LONG retention_failed_log_count = 0;
            LONG slot = InterlockedIncrement(&retention_failed_log_count);
            if (slot <= 200) {
                append_logf(
                    "foreign retention priority: insert_failed team=%u player=%u score=%d index=%d slot_accessible=%d candidate_array=%p today=%u",
                    requester_team_id,
                    candidate->player_id,
                    candidate->score,
                    before_index,
                    kbo_ai_fa_status_candidate_slot_accessible(candidate_array, before_index),
                    (void*)candidate_array,
                    today);
            }
        }
    }

    if (protectable == 0 || protectable == already_present) {
        static volatile LONG retention_no_force_log_count = 0;
        LONG slot = InterlockedIncrement(&retention_no_force_log_count);
        if (slot <= 80) {
            append_logf(
                "foreign retention priority: no_force team=%u today=%u rights=%d considered=%d protectable=%d rejected=%d already_present=%d index=%d",
                requester_team_id,
                today,
                player_count,
                considered,
                protectable,
                rejected,
                already_present,
                insert_index);
        }
    }

    return insert_index;
}
