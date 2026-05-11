#ifndef NATIVE_SRC_AMATEUR_PLAYER_QUALITY_AMATEUR_PLAYER_QUALITY_C_INTERNAL_H
#define NATIVE_SRC_AMATEUR_PLAYER_QUALITY_AMATEUR_PLAYER_QUALITY_C_INTERNAL_H

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
#include "../../team/add_player_guard/team_add_player_guard.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_string.h"

extern KboAmateurReputationSeed g_kbo_amateur_reputation_seeds[KBO_AMATEUR_REPUTATION_SEED_MAX];
extern int g_kbo_amateur_reputation_seed_count;
extern LONG g_kbo_amateur_reputation_seed_lock;
extern LONG g_kbo_amateur_reputation_seed_loaded;
extern char g_kbo_amateur_reputation_seed_loaded_path[MAX_PATH * 3];
extern KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed[KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX];
extern KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed_hash[KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX];
extern LONG g_kbo_amateur_assignment_processed_count;
extern LONG g_kbo_amateur_assignment_processed_hash_count;
extern KboAmateurAssignmentCandidate g_kbo_amateur_assignment_high_school_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
extern KboAmateurAssignmentCandidate g_kbo_amateur_assignment_college_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
int* kbo_amateur_assignment_count_ptr_for_league(uint32_t league_id);
KboAmateurAssignmentCandidate* kbo_amateur_assignment_cache_for_league(uint32_t league_id);
extern KboAmateurAssignmentProcessed g_kbo_amateur_assignment_rejected_targets[KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX];
extern int g_kbo_amateur_assignment_high_school_count;
extern int g_kbo_amateur_assignment_college_count;
extern LONG g_kbo_amateur_assignment_rejected_target_count;
extern LONG g_kbo_amateur_assignment_candidate_lock;
extern KboAmateurResolvedTeamReputation g_kbo_amateur_resolved_team_reputations[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
extern int g_kbo_amateur_resolved_team_reputation_count;
extern uint32_t g_kbo_amateur_reputation_last_update_high_school_year;
extern uint32_t g_kbo_amateur_reputation_last_update_college_year;

void kbo_lock_amateur_reputation_seeds(void);
void kbo_unlock_amateur_reputation_seeds(void);
void kbo_lock_amateur_assignment_candidates(void);
void kbo_unlock_amateur_assignment_candidates(void);
void kbo_invalidate_amateur_assignment_candidate_cache(void);
int kbo_get_reputation_seed_path(const char* file_name, char* out, size_t out_size);
int kbo_amateur_reputation_add_seed(
    uint32_t league_id,
    uint32_t team_id,
    const char* team_abbr,
    const char* team_name,
    const char* nick_name,
    uint32_t reputation);
int kbo_load_reputation_seed_path_into_cache(const char* path);
int kbo_get_amateur_reputation_history_path(char* out, size_t out_size);
int kbo_amateur_reputation_history_has_year(uint32_t league_id, uint32_t year);
int kbo_apply_amateur_reputation_history_to_cache(void);
void kbo_ensure_amateur_reputation_seeds_loaded(void);
int kbo_find_amateur_team_reputation_for_league(uint32_t league_id, uint32_t team_id, uint8_t* out_reputation);
int kbo_find_amateur_team_reputation_by_memory_team(uint32_t league_id, uint8_t* team, uint8_t* out_reputation);
uint32_t kbo_resolve_amateur_assignment_league_id_for_team_ptr(uint8_t* team);
uint32_t kbo_resolve_amateur_assignment_league_id_for_team_and_player(uint8_t* team, uint8_t* player);
int kbo_compare_amateur_reputation_update_rows(const void* a, const void* b);
uint8_t kbo_amateur_reputation_clamp_for_league(uint32_t league_id, int32_t value);
int kbo_append_amateur_reputation_history(
    uint32_t league_id,
    const KboAmateurReputationUpdateRow* rows,
    int row_count,
    const char* source,
    uint32_t year);
int kbo_update_amateur_reputation_for_league(uint32_t league_id, const char* source, uint32_t year);
void kbo_update_amateur_reputation_from_team_records(const char* source);
uint32_t kbo_amateur_assignment_processed_hash_key(uint32_t player_id, uint32_t team_id);
void kbo_amateur_assignment_clear_processed_cache(void);
int kbo_amateur_assignment_already_processed(uint32_t player_id, uint32_t team_id);
void kbo_amateur_assignment_mark_processed(uint32_t player_id, uint32_t team_id);
int kbo_amateur_player_is_hitter(uint8_t* player);
uint32_t kbo_amateur_player_assignment_league_id(uint8_t* player);
uint32_t kbo_amateur_player_assignment_team_id(uint8_t* player);
int kbo_amateur_player_position_bucket(uint8_t* player);
const char* kbo_amateur_position_bucket_label(int bucket);
int32_t kbo_amateur_assignment_target_max_players(uint32_t league_id);
void kbo_read_amateur_quality_fields(
    uint8_t* player,
    int16_t* out_overall,
    int16_t* out_talent,
    int16_t* out_ratings,
    int16_t* out_career);
int32_t kbo_amateur_quality_score(uint8_t* player);
int32_t kbo_amateur_assignment_target_reputation(uint32_t league_id, int32_t quality_score);
uint32_t kbo_amateur_assignment_pick_hash(uint32_t player_id, uint32_t league_id, int32_t quality_score, int32_t target);
int kbo_amateur_assignment_target_rejected(uint32_t league_id, uint32_t team_id);
void kbo_amateur_assignment_mark_rejected_target(uint32_t league_id, uint32_t team_id);
int kbo_amateur_assignment_team_tier(uint32_t league_id, uint8_t reputation);
int kbo_amateur_assignment_player_tier(uint32_t league_id, int32_t quality_score);
int kbo_amateur_assignment_tier_allowed(int player_tier, int team_tier);
int kbo_amateur_assignment_effective_player_tier(int player_tier, int max_team_tier);
int kbo_amateur_assignment_debug_csv_empty(HANDLE file);
void kbo_amateur_assignment_debug_write_text(HANDLE file, const char* text);
void kbo_amateur_assignment_debug_write_csv_text(HANDLE file, const char* text);
void kbo_amateur_assignment_append_debug_csv(
    const char* phase,
    const char* source,
    uint32_t player_id,
    uint32_t league_id,
    int16_t age,
    int32_t quality_score,
    int player_tier,
    uint32_t from_team_id,
    uint8_t from_reputation,
    int from_tier,
    int32_t from_player_count,
    uint32_t to_team_id,
    uint8_t to_reputation,
    int to_tier,
    int32_t to_player_count,
    int32_t target_player_count,
    uint8_t target_reputation,
    int original_result,
    uint32_t after_team_id,
    uint32_t after_league_id);
uint32_t kbo_amateur_assignment_candidate_weight(
    uint32_t league_id,
    int32_t quality_score,
    int assignment_tier,
    int32_t target,
    uint8_t reputation,
    int32_t player_count,
    int32_t target_player_count);
int kbo_amateur_assignment_collect_candidates(
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int max_candidates);
void kbo_amateur_assignment_refresh_player_counts(
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int candidate_count);
void kbo_amateur_assignment_note_player_count_delta(uint32_t league_id, uint32_t team_id, uint8_t* player, int32_t delta);
int kbo_amateur_assignment_get_cached_candidates(
    uint32_t league_id,
    KboAmateurAssignmentCandidate** out_candidates);
int kbo_amateur_assignment_find_candidate_info(
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* out_reputation,
    int* out_tier,
    int32_t* out_player_count,
    int32_t* out_hitter_count);
uint8_t* kbo_choose_amateur_assignment_team(
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    uint8_t* out_target_reputation);
uint8_t* kbo_choose_amateur_assignment_team_ortools(
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    uint8_t* out_target_reputation);
void kbo_prepare_amateur_assignment_batch_ortools(uintptr_t player_list_ptr, int32_t player_count, uintptr_t source_team_ptr);
int kbo_amateur_generation_team_add_caller(uint32_t caller_rva);
uintptr_t kbo_amateur_team_add_player_reroute_before_original(uintptr_t team_ptr, uintptr_t player_ptr, const char* source);
void kbo_amateur_team_add_player_note_original_success(uintptr_t team_ptr, uintptr_t player_ptr, const char* source, int original_result);
int kbo_amateur_defer_team_add_if_generation(
    uint32_t caller_rva,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8);
void ootp_kbo_amateur_assignment_batch_probe(uintptr_t player_list_ptr, int32_t player_count, uintptr_t source_team_ptr);
int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age);

#endif
