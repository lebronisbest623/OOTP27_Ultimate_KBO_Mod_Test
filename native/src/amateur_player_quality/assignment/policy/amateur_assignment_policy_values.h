#ifndef KBOFIX_SRC_AMATEUR_ASSIGNMENT_POLICY_VALUES_H_
#define KBOFIX_SRC_AMATEUR_ASSIGNMENT_POLICY_VALUES_H_

#include <stdint.h>

typedef struct KboAmateurPlayerQualityPolicy {
    int32_t default_team_reputation;
    int32_t quality_tier5_cutoff;
    int32_t quality_tier4_cutoff;
    int32_t quality_tier3_cutoff;
    int32_t quality_tier2_cutoff;
    int32_t quality_tier1_cutoff;
    int32_t target_reputation_tier5;
    int32_t target_reputation_tier4;
    int32_t target_reputation_tier3;
    int32_t target_reputation_tier2;
    int32_t target_reputation_tier1;
    int32_t target_reputation_tier0;
    int32_t college_target_reputation_adjustment;
    int32_t target_reputation_min;
    int32_t target_reputation_max;
    int32_t college_team_tier5_rep_min;
    int32_t college_team_tier4_rep_min;
    int32_t college_team_tier3_rep_min;
    int32_t college_team_tier2_rep_min;
    int32_t college_team_tier1_rep_min;
    int32_t high_school_team_tier5_rep_min;
    int32_t high_school_team_tier4_rep_min;
    int32_t high_school_team_tier3_rep_min;
    int32_t high_school_team_tier2_rep_min;
    int32_t high_school_team_tier1_rep_min;
    int32_t college_age_min;
    int32_t college_age_max;
    int32_t high_school_age_min;
    int32_t high_school_age_max;
    int32_t reputation_update_min_games;
    int32_t high_school_assignment_source_min_players;
    int32_t college_assignment_source_min_players;
    int32_t assignment_source_min_hitters;
    int32_t high_school_assignment_target_max_players;
    int32_t college_assignment_target_max_players;
    int32_t college_reputation_min;
    int32_t college_reputation_max;
    int32_t high_school_reputation_min;
    int32_t high_school_reputation_max;
    int32_t reputation_elite_score_min;
    int32_t reputation_elite_delta;
    int32_t reputation_strong_score_min;
    int32_t reputation_strong_delta;
    int32_t reputation_positive_score_min;
    int32_t reputation_positive_delta;
    int32_t reputation_poor_score_max;
    int32_t reputation_poor_delta;
    int32_t reputation_weak_score_max;
    int32_t reputation_weak_delta;
    int32_t reputation_negative_score_max;
    int32_t reputation_negative_delta;
    int32_t reputation_top_major_rank_count;
    int32_t reputation_top_major_bonus;
    int32_t reputation_top_minor_rank_count;
    int32_t reputation_top_minor_bonus;
    int32_t reputation_bottom_major_rank_count;
    int32_t reputation_bottom_major_penalty;
    int32_t reputation_bottom_minor_rank_count;
    int32_t reputation_bottom_minor_penalty;
    int32_t ortools_batch_idle_ms;
    int32_t ortools_batch_near_complete_idle_ms;
    int32_t ortools_batch_near_complete_teams;
    int32_t ortools_batch_near_complete_players;
} KboAmateurPlayerQualityPolicy;

const KboAmateurPlayerQualityPolicy* kbo_amateur_player_quality_policy(void);
uint8_t kbo_amateur_default_team_reputation(void);

#endif
