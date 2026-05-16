#include "captain_selection_score.h"
#include "../captain_selection_policy.h"

int32_t kbo_captain_salary_score(int32_t salary)
{
    if (salary <= 0) {
        return 0;
    }

    const KboCaptainSelectionPolicy* policy = kbo_captain_selection_policy();
    int32_t score = salary / policy->salary_score_divisor;
    if (score > policy->salary_score_max) {
        score = policy->salary_score_max;
    }
    return score;
}

int32_t kbo_captain_age_score(uint16_t age)
{
    const KboCaptainSelectionPolicy* policy = kbo_captain_selection_policy();
    if ((int32_t)age >= policy->age_core_min && (int32_t)age <= policy->age_core_max) {
        return policy->age_core_score;
    }
    if ((int32_t)age >= policy->age_extended_min && (int32_t)age <= policy->age_extended_max) {
        return policy->age_extended_score;
    }
    if ((int32_t)age >= policy->age_depth_min && (int32_t)age <= policy->age_depth_max) {
        return policy->age_depth_score;
    }
    return 0;
}

int32_t kbo_captain_same_team_seasons_score(int32_t same_team_seasons)
{
    const KboCaptainSelectionPolicy* policy = kbo_captain_selection_policy();
    if (same_team_seasons <= 0) {
        return -policy->same_team_unknown_penalty;
    }
    if (same_team_seasons < policy->same_team_min_seasons) {
        return -policy->same_team_short_penalty;
    }

    int32_t score = same_team_seasons * policy->same_team_bonus_per_season;
    if (score > policy->same_team_bonus_max) {
        score = policy->same_team_bonus_max;
    }
    return score;
}

int32_t kbo_captain_candidate_score(KboCaptainSelectionRow* row)
{
    const KboCaptainSelectionPolicy* policy = kbo_captain_selection_policy();
    int32_t score = 0;
    score += row->value_score;
    score += kbo_captain_salary_score(row->salary);
    score += kbo_captain_age_score(row->age);
    score += row->domestic ? policy->domestic_bonus : -policy->foreign_penalty;
    score += row->active_team_id == row->team_id ? policy->active_team_bonus : 0;
    score += row->current_team_id == row->team_id ? policy->current_team_bonus : 0;
    score += kbo_captain_same_team_seasons_score(row->same_team_seasons);
    score -= row->dfa ? policy->dfa_penalty : 0;
    score -= row->restricted ? policy->restricted_penalty : 0;
    score -= row->injured ? policy->injured_penalty : 0;
    return score;
}

int kbo_captain_candidate_should_replace(
    const KboCaptainSelectionRow* current,
    const KboCaptainSelectionRow* candidate)
{
    if (current == NULL || current->player_id == 0u) {
        return 1;
    }
    if (candidate == NULL || candidate->player_id == 0u) {
        return 0;
    }
    if (candidate->seeded != current->seeded) {
        return candidate->seeded != 0u;
    }
    if (candidate->seeded && candidate->seed_priority != current->seed_priority) {
        return candidate->seed_priority > current->seed_priority;
    }
    if (candidate->score != current->score) {
        return candidate->score > current->score;
    }
    return candidate->player_id < current->player_id;
}

