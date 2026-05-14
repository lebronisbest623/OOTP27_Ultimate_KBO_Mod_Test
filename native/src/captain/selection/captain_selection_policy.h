#ifndef KBOFIX_SRC_CAPTAIN_SELECTION_POLICY_H_
#define KBOFIX_SRC_CAPTAIN_SELECTION_POLICY_H_

#include <stdint.h>

typedef struct KboCaptainSelectionPolicy {
    int32_t eligible_age_min;
    int32_t eligible_age_max;
    int32_t salary_score_divisor;
    int32_t salary_score_max;
    int32_t age_core_min;
    int32_t age_core_max;
    int32_t age_core_score;
    int32_t age_extended_min;
    int32_t age_extended_max;
    int32_t age_extended_score;
    int32_t age_depth_min;
    int32_t age_depth_max;
    int32_t age_depth_score;
    int32_t domestic_bonus;
    int32_t foreign_penalty;
    int32_t active_team_bonus;
    int32_t current_team_bonus;
    int32_t same_team_min_seasons;
    int32_t same_team_bonus_per_season;
    int32_t same_team_bonus_max;
    int32_t same_team_short_penalty;
    int32_t same_team_unknown_penalty;
    int32_t dfa_penalty;
    int32_t restricted_penalty;
    int32_t injured_penalty;
} KboCaptainSelectionPolicy;

const KboCaptainSelectionPolicy* kbo_captain_selection_policy(void);

#endif
