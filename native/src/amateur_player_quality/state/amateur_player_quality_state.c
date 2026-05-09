#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../csv/amateur_reputation_csv.h"
#include "../api/amateur_player_quality.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_string.h"
#include "../internal/amateur_player_quality_internal.h"

KboAmateurReputationSeed g_kbo_amateur_reputation_seeds[KBO_AMATEUR_REPUTATION_SEED_MAX];
int g_kbo_amateur_reputation_seed_count = 0;
LONG g_kbo_amateur_reputation_seed_lock = 0;
LONG g_kbo_amateur_reputation_seed_loaded = 0;
char g_kbo_amateur_reputation_seed_loaded_path[MAX_PATH * 3] = {0};
KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed[KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX];
KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed_hash[KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX];
LONG g_kbo_amateur_assignment_processed_count = 0;
LONG g_kbo_amateur_assignment_processed_hash_count = 0;
KboAmateurAssignmentCandidate g_kbo_amateur_assignment_high_school_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
KboAmateurAssignmentCandidate g_kbo_amateur_assignment_college_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
KboAmateurAssignmentProcessed g_kbo_amateur_assignment_rejected_targets[KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX];
int g_kbo_amateur_assignment_high_school_count = -1;
int g_kbo_amateur_assignment_college_count = -1;
LONG g_kbo_amateur_assignment_rejected_target_count = 0;
LONG g_kbo_amateur_assignment_candidate_lock = 0;
KboAmateurResolvedTeamReputation g_kbo_amateur_resolved_team_reputations[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
int g_kbo_amateur_resolved_team_reputation_count = 0;
uint32_t g_kbo_amateur_reputation_last_update_high_school_year = 0u;
uint32_t g_kbo_amateur_reputation_last_update_college_year = 0u;

int* kbo_amateur_assignment_count_ptr_for_league(uint32_t league_id)
{
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return &g_kbo_amateur_assignment_high_school_count;
    }
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return &g_kbo_amateur_assignment_college_count;
    }
    return NULL;
}

KboAmateurAssignmentCandidate* kbo_amateur_assignment_cache_for_league(uint32_t league_id)
{
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return g_kbo_amateur_assignment_high_school_candidates;
    }
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return g_kbo_amateur_assignment_college_candidates;
    }
    return NULL;
}

int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age);

uint8_t* kbo_choose_amateur_assignment_team(
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    uint8_t* out_target_reputation)
{
    if (out_target_reputation != NULL) {
        *out_target_reputation = 0u;
    }
    if (player == NULL || league_id == 0u || current_team_id == 0u || quality_score <= 0) {
        return NULL;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1) {
        return NULL;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int32_t target = kbo_amateur_assignment_target_reputation(league_id, quality_score);
    if (out_target_reputation != NULL) {
        *out_target_reputation = (uint8_t)target;
    }

    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);
    if (player_tier > 0 && target <= (int32_t)current_reputation) {
        return NULL;
    }
    int max_team_tier = -1;
    for (int i = 0; i < count; i++) {
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        int team_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
        if (team_tier > max_team_tier) {
            max_team_tier = team_tier;
        }
    }
    int assignment_tier = kbo_amateur_assignment_effective_player_tier(player_tier, max_team_tier);

    int32_t current_player_count = 0;
    int32_t target_player_count = 0x7fffffff;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            current_player_count = candidates[i].player_count;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        if (kbo_amateur_assignment_candidate_weight(
                league_id,
                quality_score,
                assignment_tier,
                target,
                candidates[i].reputation,
                candidates[i].player_count,
                -1) == 0u) {
            continue;
        }
        if (candidates[i].player_count < target_player_count) {
            target_player_count = candidates[i].player_count;
        }
    }
    if (target_player_count == 0x7fffffff) {
        return NULL;
    }

    uint32_t total_weight = 0u;
    uint32_t current_weight = kbo_amateur_assignment_candidate_weight(
        league_id,
        quality_score,
        assignment_tier,
        target,
        current_reputation,
        current_player_count,
        target_player_count);
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            continue;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        uint32_t weight = kbo_amateur_assignment_candidate_weight(
            league_id,
            quality_score,
            assignment_tier,
            target,
            candidates[i].reputation,
            candidates[i].player_count,
            target_player_count);
        if (weight == 0u) {
            continue;
        }
        total_weight += weight;
    }

    if (total_weight == 0u) {
        return NULL;
    }

    if (current_weight > 0u) {
        return NULL;
    }

    uint32_t pick = kbo_amateur_assignment_pick_hash(player_id, league_id, quality_score, target) % total_weight;
    uint32_t seen = 0u;
    int fallback_index = -1;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == current_team_id) {
            continue;
        }
        if (player_tier > 0 && candidates[i].reputation <= current_reputation) {
            continue;
        }
        if (kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id)) {
            continue;
        }
        uint32_t weight = kbo_amateur_assignment_candidate_weight(
            league_id,
            quality_score,
            assignment_tier,
            target,
            candidates[i].reputation,
            candidates[i].player_count,
            target_player_count);
        if (weight == 0u) {
            continue;
        }
        fallback_index = i;
        if (pick < seen + weight) {
            return candidates[i].team;
        }
        seen += weight;
    }

    return fallback_index >= 0 ? candidates[fallback_index].team : NULL;
}

