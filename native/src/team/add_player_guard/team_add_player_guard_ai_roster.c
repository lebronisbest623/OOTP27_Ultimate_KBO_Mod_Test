#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../lookup/team_lookup.h"
#include "team_add_player_guard.h"
#include "team_add_player_guard_ai_roster.h"

typedef uintptr_t (__fastcall *OotpKboAiRosterSelectFn)(
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint);
typedef void (__fastcall *OotpKboAiRosterContextFlowFn)(
    uintptr_t context_ptr);
typedef void (__fastcall *OotpKboAiRosterApplySelectionFn)(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr,
    int32_t target_slot,
    int32_t roster_code);

static OotpKboAiRosterSelectFn g_kbo_ai_roster_select_trace_trampoline = NULL;
static OotpKboAiRosterContextFlowFn g_kbo_ai_roster_primary_apply_flow_trace_trampoline = NULL;
static OotpKboAiRosterApplySelectionFn g_kbo_ai_roster_apply_selection_trace_trampoline = NULL;
static volatile LONG g_kbo_ai_roster_foreign_apply_rescue_disabled_cached = -1;

#define KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET 0x108u
#define KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET 0x578u
#define KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET 0x4f8u
#define KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET 0x7b0u
#define KBO_AI_ROSTER_CONTEXT_TARGET_SLOT_TABLE_OFFSET 0x7bau
#define KBO_AI_ROSTER_CONTEXT_PTR528_OFFSET 0x528u
#define KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET 0x3ca8u
#define KBO_AI_ROSTER_SLOT_PLAYER_BASE_OFFSET 0x14d4u
#define KBO_AI_ROSTER_SLOT_CODE_BASE_OFFSET 0x14d8u
#define KBO_AI_ROSTER_SLOT_MAX 8
#define KBO_AI_ROSTER_SELECT_SCAN_LIMIT 256
#define KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET 0xfe0u
#define KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET 0xfe4u
#define KBO_AI_ROSTER_FOREIGN_F25_MIN 100u
#define KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT 16
#define KBO_AI_ROSTER_APPLY_RESCUE_SHIELD_MS 5000u
#define KBO_AI_ROSTER_DAILY_CALLUP_MAX_ATTEMPTS 24
#define KBO_AI_ROSTER_DAILY_TRIED_MAX 64

typedef struct KboAiRosterForeignCandidateSummary {
    int32_t index;
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t league_id;
    uint32_t default_team_id;
    uint32_t status24;
    uint32_t status25;
    uint32_t status26;
    uint32_t f25;
    uint32_t f62;
    uint32_t f65;
    int16_t f06;
    int32_t score_fe0;
    int32_t score_fe4;
    int16_t overall;
    int16_t talent;
    int16_t ratings;
} KboAiRosterForeignCandidateSummary;

typedef struct KboAiRosterDailyCallupScan {
    int scanned;
    int foreign_seen;
    int eligible_seen;
    int blocked_limit;
    int skipped_status;
    int skipped_not_minor;
    int skipped_no_team;
} KboAiRosterDailyCallupScan;

typedef struct KboAiRosterFlowPlayerSnapshot {
    uintptr_t ptr;
    int plausible;
    uint32_t player_id;
    int foreign;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t league_id;
    uint32_t default_team_id;
    uint32_t status24;
    uint32_t status25;
    uint32_t status26;
    uint32_t f61;
    uint32_t f62;
    uint32_t f65;
    uint32_t f68;
    uint32_t f1a;
    uint32_t f3e;
    int16_t f06;
    uint32_t fec;
    int16_t ef8;
    int32_t score_fe0;
    int32_t score_fe4;
    int16_t overall;
    int16_t ratings;
} KboAiRosterFlowPlayerSnapshot;

typedef struct KboAiRosterFlowContextSnapshot {
    uint16_t primary_slot;
    uint16_t primary_target_slot;
    uintptr_t primary_slot_block;
    KboAiRosterFlowPlayerSnapshot selected;
    KboAiRosterFlowPlayerSnapshot ptr528;
} KboAiRosterFlowContextSnapshot;

typedef struct KboAiRosterApplyRescueSlot {
    uintptr_t context_ptr;
    uintptr_t slot_block_ptr;
    uintptr_t player_ptr;
    uint32_t player_id;
    uint16_t slot_index;
    uint16_t target_slot;
    DWORD tick;
} KboAiRosterApplyRescueSlot;

static KboAiRosterApplyRescueSlot g_kbo_ai_roster_apply_rescue_slots[KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT];
static volatile LONG g_kbo_ai_roster_apply_rescue_slot_next = 0;

static int kbo_ai_roster_foreign_apply_rescue_enabled(void)
{
    LONG disabled = g_kbo_ai_roster_foreign_apply_rescue_disabled_cached;
    if (disabled < 0) {
        disabled = read_kbo_localappdata_flag_file("disable_ai_roster_foreign_apply_rescue.txt") ? 1 : 0;
        InterlockedCompareExchange(&g_kbo_ai_roster_foreign_apply_rescue_disabled_cached, disabled, -1);
        disabled = g_kbo_ai_roster_foreign_apply_rescue_disabled_cached;
    }
    return kbo_custom_foreign_policy_enabled() && disabled != 1;
}

static int kbo_ai_roster_foreign_apply_rescue_team_add_enabled(void)
{
    return kbo_ai_roster_foreign_apply_rescue_enabled()
        && !read_kbo_localappdata_flag_file("disable_ai_roster_foreign_apply_rescue_team_add.txt");
}

static int kbo_ai_roster_foreign_source_select_rescue_enabled(void)
{
    return kbo_custom_foreign_policy_enabled()
        && !read_kbo_localappdata_flag_file("disable_ai_roster_foreign_source_select_rescue.txt");
}

static int32_t kbo_pointer_vector_count(uintptr_t vector_ptr)
{
    if (vector_ptr == 0u || !memory_range_readable((void*)vector_ptr, 0x18u)) {
        return -1;
    }

    uint8_t* vector = (uint8_t*)vector_ptr;
    int32_t count = *(int32_t*)(vector + 0x0cu);
    if (count < 0 || count > 10000) {
        return -1;
    }

    uintptr_t values = *(uintptr_t*)vector;
    if (count > 0 && (values == 0u || !memory_range_readable((void*)values, (SIZE_T)count * sizeof(uintptr_t)))) {
        return -1;
    }
    return count;
}

static uintptr_t kbo_pointer_vector_value_at(uintptr_t vector_ptr, int32_t index, int32_t count)
{
    if (vector_ptr == 0u
            || index < 0
            || count <= 0
            || index >= count
            || count > 10000
            || !memory_range_readable((void*)vector_ptr, 0x18u)) {
        return 0u;
    }

    uintptr_t values = *(uintptr_t*)vector_ptr;
    if (values == 0u || !memory_range_readable((void*)values, (SIZE_T)count * sizeof(uintptr_t))) {
        return 0u;
    }
    return ((uintptr_t*)values)[index];
}

static int32_t kbo_read_ai_roster_select_score(uintptr_t player_ptr, uint32_t offset)
{
    if (player_ptr == 0u
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(player_ptr + offset);
}

static void kbo_read_player_default_status(
    uint8_t* player,
    uint32_t* out_status24,
    uint32_t* out_status25,
    uint32_t* out_status26)
{
    if (out_status24 != NULL) {
        *out_status24 = 0u;
    }
    if (out_status25 != NULL) {
        *out_status25 = 0u;
    }
    if (out_status26 != NULL) {
        *out_status26 = 0u;
    }
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        return;
    }

    uintptr_t status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    if (status_ptr == 0u || !memory_range_readable((void*)status_ptr, 0x29u)) {
        return;
    }

    uint8_t* status = (uint8_t*)status_ptr;
    if (out_status24 != NULL) {
        *out_status24 = status[0x24u];
    }
    if (out_status25 != NULL) {
        *out_status25 = status[0x25u];
    }
    if (out_status26 != NULL) {
        *out_status26 = status[0x26u];
    }
}

static void kbo_fill_ai_roster_foreign_candidate_summary(
    KboAiRosterForeignCandidateSummary* summary,
    int32_t index,
    uint8_t* player)
{
    if (summary == NULL) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->index = -1;

    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    summary->index = index;
    summary->player_ptr = (uintptr_t)player;
    summary->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    summary->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    summary->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    summary->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    summary->league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        summary->default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    kbo_read_player_default_status(player, &summary->status24, &summary->status25, &summary->status26);
    summary->f25 = (uint32_t)player[0xf25u];
    summary->f62 = (uint32_t)player[0xf62u];
    summary->f65 = (uint32_t)player[0xf65u];
    summary->f06 = kbo_read_player_i16(player, 0xf06u);
    summary->score_fe0 = kbo_read_ai_roster_select_score(summary->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
    summary->score_fe4 = kbo_read_ai_roster_select_score(summary->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
    summary->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    summary->talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    summary->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
}

static int64_t kbo_ai_roster_source_select_rescue_score(
    const KboAiRosterForeignCandidateSummary* summary,
    uint8_t* player)
{
    if (summary == NULL || player == NULL) {
        return INT64_MIN;
    }

    int64_t score = 0;
    score += (int64_t)summary->overall * 6;
    score += (int64_t)summary->ratings * 4;
    score += (int64_t)summary->talent * 2;
    score += (int64_t)summary->score_fe4 * 150;
    score += (int64_t)summary->score_fe0 * 80;
    score += (int64_t)summary->f25 * 12;
    score += (int64_t)kbo_read_player_i16(player, 0xf06u) * 8;
    score -= (int64_t)summary->status26 * 40;
    return score;
}

static int kbo_ai_roster_daily_player_already_tried(
    uint32_t player_id,
    const uint32_t* tried_player_ids,
    int tried_count)
{
    if (player_id == 0u || tried_player_ids == NULL || tried_count <= 0) {
        return 0;
    }
    for (int i = 0; i < tried_count; i++) {
        if (tried_player_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static int kbo_ai_roster_daily_candidate_status_ok(uint8_t* player, const KboAiRosterForeignCandidateSummary* summary)
{
    if (player == NULL || summary == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    if (*(uint8_t*)(player + OOTP27_PLAYER_RETIRED_FLAG_OFFSET) != 0u
            || *(uint8_t*)(player + OOTP27_PLAYER_DFA_FLAG_OFFSET) != 0u
            || *(uint8_t*)(player + OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET) != 0u
            || *(uint8_t*)(player + OOTP27_PLAYER_INJURY_ACTIVE_OFFSET) != 0u
            || summary->f25 < KBO_AI_ROSTER_FOREIGN_F25_MIN
            || summary->f65 != 0u
            || (summary->status26 != 0u && summary->status26 != 1u)) {
        return 0;
    }
    return 1;
}

static uint32_t kbo_ai_roster_parent_team_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            return parent_team_id;
        }
    }
    return team_id;
}

static uint8_t* kbo_ai_roster_resolve_active_team(uint8_t* player, uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return NULL;
    }

    uint32_t team_ids[3] = {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
    };
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    for (uint32_t i = 0u; i < 3u; i++) {
        uint32_t org_team_id = kbo_ai_roster_parent_team_id(team_ids[i]);
        if (org_team_id == 0u) {
            continue;
        }

        uint8_t* team = find_kbo_team_by_numeric_id_any_league(org_team_id, 1);
        if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (kbo_league_id != 0u && team_league_id != kbo_league_id) {
            continue;
        }

        if (out_team_id != NULL) {
            *out_team_id = org_team_id;
        }
        return team;
    }
    return NULL;
}

static int kbo_ai_roster_minor_foreign_callup_allows(
    int32_t team_arg,
    uint8_t* player,
    uint32_t* out_team_id,
    int* out_allowed)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_allowed != NULL) {
        *out_allowed = 0;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int minor_league_match = kbo_league_id != 0u
        && (current_league_id == kbo_league_id + 1u || (uint32_t)team_arg == kbo_league_id + 1u);
    if (!minor_league_match) {
        return 0;
    }

    uint32_t active_team_id = 0u;
    uint8_t* active_team = kbo_ai_roster_resolve_active_team(player, &active_team_id);
    if (active_team == NULL) {
        return 0;
    }

    uint8_t allowed = kbo_custom_foreign_policy_callup_allows(
        (uintptr_t)active_team,
        (uintptr_t)player,
        0,
        (int32_t)KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
        3);
    if (out_team_id != NULL) {
        *out_team_id = active_team_id;
    }
    if (out_allowed != NULL) {
        *out_allowed = allowed ? 1 : 0;
    }
    return allowed ? 1 : 0;
}

static uintptr_t kbo_ai_roster_choose_daily_callup_candidate(
    uintptr_t player_vector,
    int32_t player_count,
    const uint32_t* tried_player_ids,
    int tried_count,
    uint32_t* out_active_team_id,
    int64_t* out_score,
    KboAiRosterForeignCandidateSummary* out_summary,
    KboAiRosterDailyCallupScan* out_scan)
{
    if (out_active_team_id != NULL) {
        *out_active_team_id = 0u;
    }
    if (out_score != NULL) {
        *out_score = 0;
    }
    if (out_summary != NULL) {
        memset(out_summary, 0, sizeof(*out_summary));
        out_summary->index = -1;
    }
    if (out_scan != NULL) {
        memset(out_scan, 0, sizeof(*out_scan));
    }

    if (player_vector == 0u
            || player_count <= 0
            || player_count > 200000
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return 0u;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        return 0u;
    }

    uintptr_t best_ptr = 0u;
    uint32_t best_active_team_id = 0u;
    int64_t best_score = INT64_MIN;
    KboAiRosterForeignCandidateSummary best_summary = {0};
    best_summary.index = -1;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t candidate_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(candidate_ptr)
                || !memory_range_readable((void*)candidate_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        if (out_scan != NULL) {
            out_scan->scanned++;
        }

        uint8_t* candidate = (uint8_t*)candidate_ptr;
        if (!kbo_player_is_foreign_for_kbo_rights(candidate)) {
            continue;
        }
        if (out_scan != NULL) {
            out_scan->foreign_seen++;
        }

        KboAiRosterForeignCandidateSummary summary;
        kbo_fill_ai_roster_foreign_candidate_summary(&summary, i, candidate);
        if (summary.player_id == 0u
                || kbo_ai_roster_daily_player_already_tried(summary.player_id, tried_player_ids, tried_count)) {
            continue;
        }

        if (!kbo_ai_roster_daily_candidate_status_ok(candidate, &summary)) {
            if (out_scan != NULL) {
                out_scan->skipped_status++;
            }
            continue;
        }

        if (summary.league_id != kbo_league_id + 1u) {
            if (out_scan != NULL) {
                out_scan->skipped_not_minor++;
            }
            continue;
        }

        uint32_t active_team_id = 0u;
        int callup_allowed = 0;
        int allows = kbo_ai_roster_minor_foreign_callup_allows(
            (int32_t)summary.league_id,
            candidate,
            &active_team_id,
            &callup_allowed);
        if (active_team_id == 0u) {
            if (out_scan != NULL) {
                out_scan->skipped_no_team++;
            }
            continue;
        }
        if (!allows || !callup_allowed) {
            if (out_scan != NULL) {
                out_scan->blocked_limit++;
            }
            continue;
        }

        if (summary.current_team_id == active_team_id
                && summary.active_team_id == active_team_id
                && summary.league_id == kbo_league_id) {
            continue;
        }

        if (out_scan != NULL) {
            out_scan->eligible_seen++;
        }

        int64_t score = kbo_ai_roster_source_select_rescue_score(&summary, candidate);
        if (score > best_score) {
            best_score = score;
            best_ptr = candidate_ptr;
            best_active_team_id = active_team_id;
            best_summary = summary;
        }
    }

    if (best_ptr == 0u) {
        return 0u;
    }
    if (out_active_team_id != NULL) {
        *out_active_team_id = best_active_team_id;
    }
    if (out_score != NULL) {
        *out_score = best_score;
    }
    if (out_summary != NULL) {
        *out_summary = best_summary;
    }
    return best_ptr;
}

int kbo_run_foreign_ai_roster_daily_callup(const char* source)
{
    if (!kbo_ai_roster_foreign_apply_rescue_team_add_enabled()
            || read_kbo_localappdata_flag_file("disable_ai_roster_foreign_daily_callup.txt")) {
        return 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        static volatile LONG no_vector_log_count = 0;
        if (InterlockedIncrement(&no_vector_log_count) <= 5) {
            append_logf(
                "ootp ai roster foreign daily callup skipped source=%s reason=no_player_vector",
                source != NULL ? source : "");
        }
        return 0;
    }

    uint32_t tried_player_ids[KBO_AI_ROSTER_DAILY_TRIED_MAX] = {0};
    int tried_count = 0;
    int attempts = 0;
    int applied = 0;
    int failed = 0;
    KboAiRosterDailyCallupScan total_scan = {0};

    while (attempts < KBO_AI_ROSTER_DAILY_CALLUP_MAX_ATTEMPTS
            && tried_count < KBO_AI_ROSTER_DAILY_TRIED_MAX) {
        uint32_t active_team_id = 0u;
        int64_t score = 0;
        KboAiRosterForeignCandidateSummary before;
        KboAiRosterDailyCallupScan scan;
        uintptr_t player_ptr = kbo_ai_roster_choose_daily_callup_candidate(
            player_vector,
            player_count,
            tried_player_ids,
            tried_count,
            &active_team_id,
            &score,
            &before,
            &scan);

        total_scan.scanned += scan.scanned;
        total_scan.foreign_seen += scan.foreign_seen;
        total_scan.eligible_seen += scan.eligible_seen;
        total_scan.blocked_limit += scan.blocked_limit;
        total_scan.skipped_status += scan.skipped_status;
        total_scan.skipped_not_minor += scan.skipped_not_minor;
        total_scan.skipped_no_team += scan.skipped_no_team;

        if (player_ptr == 0u || before.player_id == 0u || active_team_id == 0u) {
            break;
        }

        tried_player_ids[tried_count++] = before.player_id;
        attempts++;

        uint8_t* active_team = find_kbo_team_by_numeric_id_any_league(active_team_id, 1);
        if (active_team == NULL || !memory_range_readable(active_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            failed++;
            continue;
        }

        uint32_t team_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint32_t team_league_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (team_id == 0u || team_id != active_team_id || team_league_id == 0u) {
            failed++;
            continue;
        }

        uint8_t result = kbo_team_add_player_guard_call_original(
            (uintptr_t)active_team,
            player_ptr,
            0u,
            0u,
            0u,
            0u,
            0u,
            0u);

        KboAiRosterForeignCandidateSummary after;
        kbo_fill_ai_roster_foreign_candidate_summary(&after, before.index, (uint8_t*)player_ptr);
        int moved_to_major = result != 0u
            && after.current_team_id == active_team_id
            && after.active_team_id == active_team_id
            && after.league_id == team_league_id;
        if (moved_to_major) {
            applied++;
        } else {
            failed++;
        }

        static volatile LONG detail_log_count = 0;
        LONG detail_slot = InterlockedIncrement(&detail_log_count);
        if (detail_slot <= 240) {
            append_logf(
                "ootp ai roster foreign daily callup #%ld source=%s attempt=%d result=%u moved=%d team=%u team_league=%u player=%u nation=%u score=%lld current=%u->%u active=%u->%u league=%u->%u status24=%u->%u status25=%u->%u status26=%u->%u f25=%u f62=%u->%u f65=%u->%u overall=%d ratings=%d",
                detail_slot,
                source != NULL ? source : "",
                attempts,
                (uint32_t)result,
                moved_to_major,
                active_team_id,
                team_league_id,
                before.player_id,
                before.nation_id,
                (long long)score,
                before.current_team_id,
                after.current_team_id,
                before.active_team_id,
                after.active_team_id,
                before.league_id,
                after.league_id,
                before.status24,
                after.status24,
                before.status25,
                after.status25,
                before.status26,
                after.status26,
                before.f25,
                before.f62,
                after.f62,
                before.f65,
                after.f65,
                after.overall,
                after.ratings);
        }
    }

    static volatile LONG summary_log_count = 0;
    LONG summary_slot = InterlockedIncrement(&summary_log_count);
    if (summary_slot <= 1000 && (applied > 0 || failed > 0 || total_scan.eligible_seen > 0)) {
        append_logf(
            "ootp ai roster foreign daily callup summary #%ld source=%s attempts=%d applied=%d failed=%d scanned=%d foreign=%d eligible=%d blocked_limit=%d skipped_status=%d skipped_not_minor=%d skipped_no_team=%d",
            summary_slot,
            source != NULL ? source : "",
            attempts,
            applied,
            failed,
            total_scan.scanned,
            total_scan.foreign_seen,
            total_scan.eligible_seen,
            total_scan.blocked_limit,
            total_scan.skipped_status,
            total_scan.skipped_not_minor,
            total_scan.skipped_no_team);
    }
    return applied;
}

static uintptr_t kbo_ai_roster_choose_source_select_rescue_candidate(
    uintptr_t source_vector_ptr,
    int32_t source_count,
    uintptr_t native_result_ptr,
    int32_t* out_source_index,
    uint32_t* out_active_team_id,
    int64_t* out_score,
    KboAiRosterForeignCandidateSummary* out_summary)
{
    if (out_source_index != NULL) {
        *out_source_index = -1;
    }
    if (out_active_team_id != NULL) {
        *out_active_team_id = 0u;
    }
    if (out_score != NULL) {
        *out_score = 0;
    }
    if (out_summary != NULL) {
        memset(out_summary, 0, sizeof(*out_summary));
        out_summary->index = -1;
    }

    if (!kbo_ai_roster_foreign_source_select_rescue_enabled()
            || source_vector_ptr == 0u
            || source_count <= 0) {
        return 0u;
    }

    if (kbo_player_pointer_plausible(native_result_ptr)
            && memory_range_readable((void*)native_result_ptr, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights((uint8_t*)native_result_ptr)) {
        return 0u;
    }

    int32_t scanned_count = source_count > KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        ? KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        : source_count;
    uintptr_t best_ptr = 0u;
    int32_t best_index = -1;
    uint32_t best_active_team_id = 0u;
    int64_t best_score = INT64_MIN;
    KboAiRosterForeignCandidateSummary best_summary = {0};
    best_summary.index = -1;
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();

    for (int32_t i = 0; i < scanned_count; i++) {
        uintptr_t candidate_ptr = kbo_pointer_vector_value_at(source_vector_ptr, i, source_count);
        if (!kbo_player_pointer_plausible(candidate_ptr)
                || !memory_range_readable((void*)candidate_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        uint8_t* candidate = (uint8_t*)candidate_ptr;
        if (!kbo_player_is_foreign_for_kbo_rights(candidate)) {
            continue;
        }

        KboAiRosterForeignCandidateSummary summary;
        kbo_fill_ai_roster_foreign_candidate_summary(&summary, i, candidate);
        if (summary.player_id == 0u
                || summary.f25 < KBO_AI_ROSTER_FOREIGN_F25_MIN
                || summary.f65 != 0u
                || (summary.status26 != 0u && summary.status26 != 1u)) {
            continue;
        }

        uint32_t active_team_id = 0u;
        int callup_allowed = 0;
        if (!kbo_ai_roster_minor_foreign_callup_allows(
                (int32_t)summary.league_id,
                candidate,
                &active_team_id,
                &callup_allowed)
                || !callup_allowed
                || active_team_id == 0u) {
            continue;
        }

        if (kbo_league_id != 0u
                && summary.current_team_id == active_team_id
                && summary.active_team_id == active_team_id
                && summary.league_id == kbo_league_id) {
            continue;
        }

        int64_t score = kbo_ai_roster_source_select_rescue_score(&summary, candidate);
        if (score > best_score) {
            best_score = score;
            best_ptr = candidate_ptr;
            best_index = i;
            best_active_team_id = active_team_id;
            best_summary = summary;
        }
    }

    if (best_ptr == 0u) {
        return 0u;
    }

    if (out_source_index != NULL) {
        *out_source_index = best_index;
    }
    if (out_active_team_id != NULL) {
        *out_active_team_id = best_active_team_id;
    }
    if (out_score != NULL) {
        *out_score = best_score;
    }
    if (out_summary != NULL) {
        *out_summary = best_summary;
    }
    return best_ptr;
}

void kbo_set_ai_roster_select_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_select_trace_trampoline = (OotpKboAiRosterSelectFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_primary_apply_flow_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_primary_apply_flow_trace_trampoline =
        (OotpKboAiRosterContextFlowFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_apply_selection_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_apply_selection_trace_trampoline =
        (OotpKboAiRosterApplySelectionFn)(uintptr_t)trampoline;
}

static uint16_t kbo_ai_roster_context_u16(uintptr_t context_ptr, uintptr_t offset)
{
    if (context_ptr == 0u || !memory_range_readable((void*)(context_ptr + offset), sizeof(uint16_t))) {
        return 0u;
    }
    return *(uint16_t*)(context_ptr + offset);
}

static uintptr_t kbo_ai_roster_context_ptr(uintptr_t context_ptr, uintptr_t offset)
{
    if (context_ptr == 0u || !memory_range_readable((void*)(context_ptr + offset), sizeof(uintptr_t))) {
        return 0u;
    }
    return *(uintptr_t*)(context_ptr + offset);
}

static uintptr_t kbo_ai_roster_context_slot_block(uintptr_t context_ptr, uint16_t slot)
{
    if (slot >= 64u) {
        return 0u;
    }
    return kbo_ai_roster_context_ptr(
        context_ptr,
        KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET + (uintptr_t)slot * sizeof(uintptr_t));
}

static uint32_t kbo_ai_roster_context_slot_team_id(uintptr_t context_ptr, uint16_t slot)
{
    if (context_ptr == 0u || slot >= 64u) {
        return 0u;
    }

    uintptr_t addr = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET
        + (uintptr_t)slot * sizeof(uint32_t);
    if (!memory_range_readable((void*)addr, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)addr;
}

static uint32_t kbo_ai_roster_slot_code_at(uintptr_t slot_block_ptr, uint16_t target_slot)
{
    if (slot_block_ptr == 0u || target_slot > KBO_AI_ROSTER_SLOT_MAX) {
        return 0u;
    }

    uintptr_t addr = slot_block_ptr + KBO_AI_ROSTER_SLOT_CODE_BASE_OFFSET
        + (uintptr_t)target_slot * 8u;
    if (!memory_range_readable((void*)addr, sizeof(uint8_t))) {
        return 0u;
    }
    return (uint32_t)*(uint8_t*)addr;
}

static uint32_t kbo_ai_roster_slot_player_at(uintptr_t slot_block_ptr, uint16_t target_slot)
{
    if (slot_block_ptr == 0u || target_slot > KBO_AI_ROSTER_SLOT_MAX) {
        return 0u;
    }

    uintptr_t addr = slot_block_ptr + KBO_AI_ROSTER_SLOT_PLAYER_BASE_OFFSET
        + (uintptr_t)target_slot * 8u;
    if (!memory_range_readable((void*)addr, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)addr;
}

static void kbo_ai_roster_flow_read_player(
    uintptr_t player_ptr,
    KboAiRosterFlowPlayerSnapshot* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->ptr = player_ptr;

    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    out->plausible = 1;
    out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out->foreign = kbo_player_is_foreign_for_kbo_rights(player);
    out->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        out->default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    kbo_read_player_default_status(player, &out->status24, &out->status25, &out->status26);
    out->f61 = (uint32_t)player[0xf61u];
    out->f62 = (uint32_t)player[0xf62u];
    out->f65 = (uint32_t)player[0xf65u];
    out->f68 = (uint32_t)player[0xf68u];
    out->f1a = (uint32_t)player[0xf1au];
    out->f3e = (uint32_t)player[0xf3eu];
    out->f06 = kbo_read_player_i16(player, 0xf06u);
    if (memory_range_readable(player + 0xfecu, sizeof(uint32_t))) {
        out->fec = *(uint32_t*)(player + 0xfecu);
    }
    out->ef8 = kbo_read_player_i16(player, 0xef8u);
    out->score_fe0 = kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
    out->score_fe4 = kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
    out->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    out->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
}

static void kbo_ai_roster_flow_read_context(
    uintptr_t context_ptr,
    KboAiRosterFlowContextSnapshot* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));

    out->primary_slot = kbo_ai_roster_context_u16(context_ptr, KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET);
    out->primary_target_slot = out->primary_slot < 64u
        ? kbo_ai_roster_context_u16(
            context_ptr,
            KBO_AI_ROSTER_CONTEXT_TARGET_SLOT_TABLE_OFFSET + (uintptr_t)out->primary_slot * sizeof(uint16_t))
        : 0u;
    out->primary_slot_block = kbo_ai_roster_context_slot_block(context_ptr, out->primary_slot);

    kbo_ai_roster_flow_read_player(
        kbo_ai_roster_context_ptr(context_ptr, KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET),
        &out->selected);
    kbo_ai_roster_flow_read_player(
        kbo_ai_roster_context_ptr(context_ptr, KBO_AI_ROSTER_CONTEXT_PTR528_OFFSET),
        &out->ptr528);
}

static void kbo_ai_roster_record_foreign_apply_rescue(
    uintptr_t context_ptr,
    uintptr_t slot_block_ptr,
    uintptr_t player_ptr,
    uint32_t player_id,
    uint16_t slot_index,
    uint16_t target_slot)
{
    if (slot_block_ptr == 0u || player_id == 0u || slot_index >= 64u || target_slot > KBO_AI_ROSTER_SLOT_MAX) {
        return;
    }

    LONG next = InterlockedIncrement(&g_kbo_ai_roster_apply_rescue_slot_next);
    LONG slot = (next - 1) % KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT;
    if (slot < 0) {
        slot += KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT;
    }

    g_kbo_ai_roster_apply_rescue_slots[slot].context_ptr = context_ptr;
    g_kbo_ai_roster_apply_rescue_slots[slot].slot_block_ptr = slot_block_ptr;
    g_kbo_ai_roster_apply_rescue_slots[slot].player_ptr = player_ptr;
    g_kbo_ai_roster_apply_rescue_slots[slot].player_id = player_id;
    g_kbo_ai_roster_apply_rescue_slots[slot].slot_index = slot_index;
    g_kbo_ai_roster_apply_rescue_slots[slot].target_slot = target_slot;
    g_kbo_ai_roster_apply_rescue_slots[slot].tick = GetTickCount();
}

static int kbo_ai_roster_recent_foreign_apply_rescue_match(
    uintptr_t context_ptr,
    uintptr_t slot_block_ptr,
    uint16_t target_slot,
    uint32_t player_id,
    DWORD* out_age_ms)
{
    if (out_age_ms != NULL) {
        *out_age_ms = 0u;
    }
    if (slot_block_ptr == 0u || target_slot > KBO_AI_ROSTER_SLOT_MAX || player_id == 0u) {
        return 0;
    }

    DWORD now = GetTickCount();
    for (int i = 0; i < KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT; i++) {
        KboAiRosterApplyRescueSlot entry = g_kbo_ai_roster_apply_rescue_slots[i];
        if (entry.player_id != player_id
                || entry.slot_block_ptr != slot_block_ptr
                || entry.target_slot != target_slot) {
            continue;
        }
        if (entry.context_ptr != 0u && context_ptr != 0u && entry.context_ptr != context_ptr) {
            continue;
        }

        DWORD age_ms = now - entry.tick;
        if (age_ms > KBO_AI_ROSTER_APPLY_RESCUE_SHIELD_MS) {
            continue;
        }

        if (out_age_ms != NULL) {
            *out_age_ms = age_ms;
        }
        return 1;
    }
    return 0;
}

static uint8_t kbo_ai_roster_context_flow_apply_rescue_team_add(
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    uint32_t active_team_id,
    int callup_allowed,
    uintptr_t slot_block_ptr,
    uint16_t target_slot)
{
    if (context_ptr == 0u
            || before == NULL
            || !before->selected.plausible
            || before->selected.ptr == 0u
            || before->selected.player_id == 0u
            || active_team_id == 0u
            || !callup_allowed
            || !kbo_ai_roster_foreign_apply_rescue_team_add_enabled()) {
        return 0u;
    }

    uint8_t* active_team = find_kbo_team_by_numeric_id_any_league(active_team_id, 1);
    if (active_team == NULL || !memory_range_readable(active_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    uint32_t team_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_id != active_team_id
            || (before->selected.current_team_id == team_id
                && before->selected.active_team_id == team_id
                && before->selected.league_id == team_league_id)) {
        return 0u;
    }

    uint8_t result = kbo_team_add_player_guard_call_original(
        (uintptr_t)active_team,
        before->selected.ptr,
        0u,
        0u,
        0u,
        0u,
        0u,
        0u);

    KboAiRosterFlowPlayerSnapshot after_player;
    kbo_ai_roster_flow_read_player(before->selected.ptr, &after_player);

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot <= 300) {
        append_logf(
            "ootp ai roster foreign apply rescue team-add #%ld result=%u context=%p slot_block=%p target_slot=%u team=%u team_league=%u player=%u foreign=%d nation=%u current=%u->%u active=%u->%u league=%u->%u status24=%u->%u status25=%u->%u status26=%u->%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u overall=%d ratings=%d",
            trace_slot,
            (uint32_t)result,
            (void*)context_ptr,
            (void*)slot_block_ptr,
            target_slot,
            team_id,
            team_league_id,
            before->selected.player_id,
            before->selected.foreign,
            before->selected.nation_id,
            before->selected.current_team_id,
            after_player.current_team_id,
            before->selected.active_team_id,
            after_player.active_team_id,
            before->selected.league_id,
            after_player.league_id,
            before->selected.status24,
            after_player.status24,
            before->selected.status25,
            after_player.status25,
            before->selected.status26,
            after_player.status26,
            before->selected.f62,
            after_player.f62,
            before->selected.f65,
            after_player.f65,
            before->selected.f68,
            after_player.f68,
            before->selected.f1a,
            after_player.f1a,
            after_player.overall,
            after_player.ratings);
    }
    return result;
}

static int kbo_ai_roster_context_flow_apply_rescue(
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    const KboAiRosterFlowContextSnapshot* after)
{
    if (context_ptr == 0u
            || before == NULL
            || after == NULL
            || g_kbo_ai_roster_apply_selection_trace_trampoline == NULL
            || !kbo_ai_roster_foreign_apply_rescue_enabled()) {
        return 0;
    }
    if (!before->selected.plausible
            || !before->selected.foreign
            || after->selected.ptr != 0u) {
        return 0;
    }
    if (before->selected.ptr != before->ptr528.ptr
            && before->selected.ptr != after->ptr528.ptr) {
        return 0;
    }
    if (before->primary_slot >= 64u
            || before->primary_target_slot > KBO_AI_ROSTER_SLOT_MAX
            || before->primary_slot_block == 0u) {
        return 0;
    }

    int team_add_rescue_enabled = kbo_ai_roster_foreign_apply_rescue_team_add_enabled();
    if (before->selected.f65 != 0u
            || (before->selected.status26 != 0u
                && (!team_add_rescue_enabled || before->selected.status26 != 1u))) {
        return 0;
    }

    uint32_t active_team_id = 0u;
    int callup_allowed = 0;
    if (!kbo_ai_roster_minor_foreign_callup_allows(
            (int32_t)before->selected.league_id,
            (uint8_t*)before->selected.ptr,
            &active_team_id,
            &callup_allowed)) {
        return 0;
    }

    uint16_t target_slot = before->primary_target_slot;
    uintptr_t slot_block_ptr = before->primary_slot_block;
    uint32_t slot_team_id = kbo_ai_roster_context_slot_team_id(context_ptr, before->primary_slot);
    uint32_t before_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, target_slot);
    uint32_t before_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, target_slot);

    g_kbo_ai_roster_apply_selection_trace_trampoline(
        context_ptr,
        (int32_t)before->primary_slot,
        before->selected.ptr,
        (int32_t)target_slot,
        11);

    uint32_t after_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, target_slot);
    uint32_t after_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, target_slot);
    if (after_slot_player_id == before->selected.player_id) {
        kbo_ai_roster_context_flow_apply_rescue_team_add(
            context_ptr,
            before,
            active_team_id,
            callup_allowed,
            slot_block_ptr,
            target_slot);
        kbo_ai_roster_record_foreign_apply_rescue(
            context_ptr,
            slot_block_ptr,
            before->selected.ptr,
            before->selected.player_id,
            before->primary_slot,
            target_slot);
    }

    KboAiRosterFlowPlayerSnapshot after_player;
    kbo_ai_roster_flow_read_player(before->selected.ptr, &after_player);

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot <= 1000) {
        append_logf(
            "ootp ai roster foreign apply rescue #%ld context=%p slot_index=%u target_slot=%u roster_code=11 slot_block=%p slot_team=%u active_team=%u before_slot_code=%u after_slot_code=%u before_slot_player=%u after_slot_player=%u player=%u foreign=%d nation=%u current=%u->%u active=%u->%u league=%u status24=%u->%u status25=%u->%u status26=%u->%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u score_fe0=%d score_fe4=%d overall=%d ratings=%d",
            trace_slot,
            (void*)context_ptr,
            before->primary_slot,
            target_slot,
            (void*)slot_block_ptr,
            slot_team_id,
            active_team_id,
            before_slot_code,
            after_slot_code,
            before_slot_player_id,
            after_slot_player_id,
            before->selected.player_id,
            before->selected.foreign,
            before->selected.nation_id,
            before->selected.current_team_id,
            after_player.current_team_id,
            before->selected.active_team_id,
            after_player.active_team_id,
            before->selected.league_id,
            before->selected.status24,
            after_player.status24,
            before->selected.status25,
            after_player.status25,
            before->selected.status26,
            after_player.status26,
            before->selected.f62,
            after_player.f62,
            before->selected.f65,
            after_player.f65,
            before->selected.f68,
            after_player.f68,
            before->selected.f1a,
            after_player.f1a,
            before->selected.score_fe0,
            before->selected.score_fe4,
            after_player.overall,
            after_player.ratings);
    }
    return 1;
}

__declspec(noinline) uintptr_t ootp_kbo_ai_roster_select_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    OotpKboAiRosterSelectFn original = g_kbo_ai_roster_select_trace_trampoline;
    uintptr_t result_ptr = original != NULL
        ? original(context_ptr, slot_index, depth_hint)
        : 0u;

    uintptr_t slot_block_ptr = 0u;
    uintptr_t source_vector_ptr = 0u;
    int32_t source_count = -1;
    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        slot_block_ptr = kbo_ai_roster_context_slot_block(context_ptr, (uint16_t)slot_index);
        if (slot_block_ptr != 0u) {
            source_vector_ptr = slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET;
            source_count = kbo_pointer_vector_count(source_vector_ptr);
        }
    }

    int32_t rescue_source_index = -1;
    uint32_t rescue_active_team_id = 0u;
    int64_t rescue_score = 0;
    KboAiRosterForeignCandidateSummary rescue_summary;
    uintptr_t rescue_ptr = kbo_ai_roster_choose_source_select_rescue_candidate(
        source_vector_ptr,
        source_count,
        result_ptr,
        &rescue_source_index,
        &rescue_active_team_id,
        &rescue_score,
        &rescue_summary);
    if (rescue_ptr == 0u || rescue_ptr == result_ptr) {
        return result_ptr;
    }

    uint32_t native_player_id = 0u;
    int native_foreign = 0;
    int32_t native_score_fe0 = 0;
    int32_t native_score_fe4 = 0;
    int16_t native_overall = 0;
    int16_t native_ratings = 0;
    if (kbo_player_pointer_plausible(result_ptr)
            && memory_range_readable((void*)result_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t* native_player = (uint8_t*)result_ptr;
        native_player_id = *(uint32_t*)(native_player + OOTP27_PLAYER_ID_OFFSET);
        native_foreign = kbo_player_is_foreign_for_kbo_rights(native_player);
        native_score_fe0 = kbo_read_ai_roster_select_score(result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
        native_score_fe4 = kbo_read_ai_roster_select_score(result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
        native_overall = kbo_read_player_i16(native_player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
        native_ratings = kbo_read_player_i16(native_player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    }

    static volatile LONG rescue_log_count = 0;
    LONG rescue_slot = InterlockedIncrement(&rescue_log_count);
    append_logf(
        "ootp ai roster foreign source-select rescue #%ld caller_rva=0x%x context=%p slot_index=%d depth_hint=%d source_count=%d native=%u native_foreign=%d native_fe0=%d native_fe4=%d native_overall=%d native_ratings=%d override=%u source_idx=%d active_team=%u score=%lld current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u f25=%u f62=%u f65=%u f06=%d fe0=%d fe4=%d overall=%d talent=%d ratings=%d",
        rescue_slot,
        caller_rva,
        (void*)context_ptr,
        slot_index,
        depth_hint,
        source_count,
        native_player_id,
        native_foreign,
        native_score_fe0,
        native_score_fe4,
        native_overall,
        native_ratings,
        rescue_summary.player_id,
        rescue_source_index,
        rescue_active_team_id,
        (long long)rescue_score,
        rescue_summary.current_team_id,
        rescue_summary.active_team_id,
        rescue_summary.league_id,
        rescue_summary.default_team_id,
        rescue_summary.status24,
        rescue_summary.status25,
        rescue_summary.status26,
        rescue_summary.f25,
        rescue_summary.f62,
        rescue_summary.f65,
        rescue_summary.f06,
        rescue_summary.score_fe0,
        rescue_summary.score_fe4,
        rescue_summary.overall,
        rescue_summary.talent,
        rescue_summary.ratings);
    return rescue_ptr;
}

__declspec(noinline) void ootp_kbo_ai_roster_primary_apply_flow_trace_wrapper(
    uintptr_t context_ptr)
{
    KboAiRosterFlowContextSnapshot before;
    KboAiRosterFlowContextSnapshot after;
    kbo_ai_roster_flow_read_context(context_ptr, &before);

    OotpKboAiRosterContextFlowFn original = g_kbo_ai_roster_primary_apply_flow_trace_trampoline;
    if (original != NULL) {
        original(context_ptr);
    }

    kbo_ai_roster_flow_read_context(context_ptr, &after);
    if (kbo_ai_roster_context_flow_apply_rescue(context_ptr, &before, &after)) {
        kbo_ai_roster_flow_read_context(context_ptr, &after);
    }
}

__declspec(noinline) void ootp_kbo_ai_roster_apply_selection_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr,
    int32_t target_slot,
    int32_t roster_code)
{
    OotpKboAiRosterApplySelectionFn original = g_kbo_ai_roster_apply_selection_trace_trampoline;

    uintptr_t slot_block_ptr = 0u;
    uint32_t slot_team_id = 0u;
    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        slot_block_ptr = kbo_ai_roster_context_slot_block(context_ptr, (uint16_t)slot_index);
        slot_team_id = kbo_ai_roster_context_slot_team_id(context_ptr, (uint16_t)slot_index);
    }

    uint32_t before_slot_code = 0u;
    uint32_t before_slot_player_id = 0u;
    if (slot_block_ptr != 0u && target_slot >= 0 && target_slot <= KBO_AI_ROSTER_SLOT_MAX) {
        before_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, (uint16_t)target_slot);
        before_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, (uint16_t)target_slot);
    }

    int player_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    uint8_t* player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t player_id = player_plausible ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) : 0u;
    int incoming_foreign = player_plausible && kbo_player_is_foreign_for_kbo_rights(player);
    DWORD rescue_age_ms = 0u;
    if (original != NULL
            && slot_block_ptr != 0u
            && target_slot >= 0
            && target_slot <= KBO_AI_ROSTER_SLOT_MAX
            && player_plausible
            && player_id != 0u
            && !incoming_foreign
            && before_slot_player_id != 0u
            && before_slot_player_id != player_id
            && kbo_ai_roster_recent_foreign_apply_rescue_match(
                context_ptr,
                slot_block_ptr,
                (uint16_t)target_slot,
                before_slot_player_id,
                &rescue_age_ms)) {
        uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
        HMODULE host_exe = GetModuleHandleA(NULL);
        uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
            ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
            : 0u;

        static volatile LONG shield_log_count = 0;
        LONG shield_slot = InterlockedIncrement(&shield_log_count);
        if (shield_slot <= 1000) {
            append_logf(
                "ootp ai roster foreign apply rescue shield #%ld caller_rva=0x%x context=%p slot_index=%d target_slot=%d roster_code=%d age_ms=%lu slot_block=%p slot_team=%u before_slot_code=%u before_slot_player=%u incoming_player=%u incoming_foreign=%d incoming_nation=%u incoming_current=%u incoming_active=%u incoming_league=%u incoming_status24=%u incoming_status25=%u incoming_status26=%u incoming_f62=%u incoming_f65=%u incoming_f68=%u incoming_f1a=%u incoming_score_fe0=%d incoming_score_fe4=%d incoming_overall=%d incoming_ratings=%d",
                shield_slot,
                caller_rva,
                (void*)context_ptr,
                slot_index,
                target_slot,
                roster_code,
                (unsigned long)rescue_age_ms,
                (void*)slot_block_ptr,
                slot_team_id,
                before_slot_code,
                before_slot_player_id,
                player_id,
                incoming_foreign,
                *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                0u,
                0u,
                0u,
                (uint32_t)player[0xf62u],
                (uint32_t)player[0xf65u],
                (uint32_t)player[0xf68u],
                (uint32_t)player[0xf1au],
                kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET),
                kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET),
                kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
                kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
        }
        return;
    }

    if (original != NULL) {
        original(context_ptr, slot_index, player_ptr, target_slot, roster_code);
    }
}
