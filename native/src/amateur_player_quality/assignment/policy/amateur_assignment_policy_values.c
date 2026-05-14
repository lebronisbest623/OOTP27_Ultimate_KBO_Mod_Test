#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "amateur_assignment_policy_values.h"

#include "../../../core/core_flags/localappdata/localappdata_reader.h"

#define KBO_AMATEUR_PLAYER_QUALITY_POLICY_FILE "amateur_player_quality_policy.json"

static INIT_ONCE g_kbo_amateur_player_quality_policy_once = INIT_ONCE_STATIC_INIT;
static KboAmateurPlayerQualityPolicy g_kbo_amateur_player_quality_policy;

static int32_t kbo_amateur_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    int value = (int)fallback;
    if (kbo_read_localappdata_named_json_int_value(KBO_AMATEUR_PLAYER_QUALITY_POLICY_FILE, key, &value)
            && value >= min_value && value <= max_value) {
        return (int32_t)value;
    }
    return fallback;
}

static BOOL CALLBACK kbo_amateur_player_quality_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboAmateurPlayerQualityPolicy* p = &g_kbo_amateur_player_quality_policy;
    p->default_team_reputation = kbo_amateur_policy_int("default_team_reputation", 70, 0, 100);
    p->quality_tier5_cutoff = kbo_amateur_policy_int("quality_tier5_cutoff", 2300, 0, 1000000);
    p->quality_tier4_cutoff = kbo_amateur_policy_int("quality_tier4_cutoff", 1800, 0, 1000000);
    p->quality_tier3_cutoff = kbo_amateur_policy_int("quality_tier3_cutoff", 1350, 0, 1000000);
    p->quality_tier2_cutoff = kbo_amateur_policy_int("quality_tier2_cutoff", 950, 0, 1000000);
    p->quality_tier1_cutoff = kbo_amateur_policy_int("quality_tier1_cutoff", 600, 0, 1000000);
    p->target_reputation_tier5 = kbo_amateur_policy_int("target_reputation_tier5", 92, 0, 100);
    p->target_reputation_tier4 = kbo_amateur_policy_int("target_reputation_tier4", 84, 0, 100);
    p->target_reputation_tier3 = kbo_amateur_policy_int("target_reputation_tier3", 72, 0, 100);
    p->target_reputation_tier2 = kbo_amateur_policy_int("target_reputation_tier2", 58, 0, 100);
    p->target_reputation_tier1 = kbo_amateur_policy_int("target_reputation_tier1", 45, 0, 100);
    p->target_reputation_tier0 = kbo_amateur_policy_int("target_reputation_tier0", 35, 0, 100);
    p->college_target_reputation_adjustment = kbo_amateur_policy_int("college_target_reputation_adjustment", -10, -100, 100);
    p->target_reputation_min = kbo_amateur_policy_int("target_reputation_min", 25, 0, 100);
    p->target_reputation_max = kbo_amateur_policy_int("target_reputation_max", 95, 0, 100);
    p->college_team_tier5_rep_min = kbo_amateur_policy_int("college_team_tier5_rep_min", 68, 0, 100);
    p->college_team_tier4_rep_min = kbo_amateur_policy_int("college_team_tier4_rep_min", 55, 0, 100);
    p->college_team_tier3_rep_min = kbo_amateur_policy_int("college_team_tier3_rep_min", 42, 0, 100);
    p->college_team_tier2_rep_min = kbo_amateur_policy_int("college_team_tier2_rep_min", 28, 0, 100);
    p->college_team_tier1_rep_min = kbo_amateur_policy_int("college_team_tier1_rep_min", 15, 0, 100);
    p->high_school_team_tier5_rep_min = kbo_amateur_policy_int("high_school_team_tier5_rep_min", 90, 0, 100);
    p->high_school_team_tier4_rep_min = kbo_amateur_policy_int("high_school_team_tier4_rep_min", 80, 0, 100);
    p->high_school_team_tier3_rep_min = kbo_amateur_policy_int("high_school_team_tier3_rep_min", 68, 0, 100);
    p->high_school_team_tier2_rep_min = kbo_amateur_policy_int("high_school_team_tier2_rep_min", 56, 0, 100);
    p->high_school_team_tier1_rep_min = kbo_amateur_policy_int("high_school_team_tier1_rep_min", 48, 0, 100);
    p->college_age_min = kbo_amateur_policy_int("college_age_min", 17, 0, 80);
    p->college_age_max = kbo_amateur_policy_int("college_age_max", 25, 0, 80);
    p->high_school_age_min = kbo_amateur_policy_int("high_school_age_min", 14, 0, 80);
    p->high_school_age_max = kbo_amateur_policy_int("high_school_age_max", 18, 0, 80);
    p->reputation_update_min_games = kbo_amateur_policy_int("reputation_update_min_games", 10, 1, 10000);
    p->high_school_assignment_source_min_players = kbo_amateur_policy_int("high_school_assignment_source_min_players", 18, 0, 1000);
    p->college_assignment_source_min_players = kbo_amateur_policy_int("college_assignment_source_min_players", 24, 0, 1000);
    p->assignment_source_min_hitters = kbo_amateur_policy_int("assignment_source_min_hitters", 9, 0, 1000);
    p->high_school_assignment_target_max_players = kbo_amateur_policy_int("high_school_assignment_target_max_players", 34, 1, 1000);
    p->college_assignment_target_max_players = kbo_amateur_policy_int("college_assignment_target_max_players", 40, 1, 1000);
    p->college_reputation_min = kbo_amateur_policy_int("college_reputation_min", 3, 0, 100);
    p->college_reputation_max = kbo_amateur_policy_int("college_reputation_max", 78, 0, 100);
    p->high_school_reputation_min = kbo_amateur_policy_int("high_school_reputation_min", 15, 0, 100);
    p->high_school_reputation_max = kbo_amateur_policy_int("high_school_reputation_max", 98, 0, 100);
    p->reputation_elite_score_min = kbo_amateur_policy_int("reputation_elite_score_min", 1600, 0, 10000);
    p->reputation_elite_delta = kbo_amateur_policy_int("reputation_elite_delta", 4, -100, 100);
    p->reputation_strong_score_min = kbo_amateur_policy_int("reputation_strong_score_min", 1300, 0, 10000);
    p->reputation_strong_delta = kbo_amateur_policy_int("reputation_strong_delta", 2, -100, 100);
    p->reputation_positive_score_min = kbo_amateur_policy_int("reputation_positive_score_min", 1100, 0, 10000);
    p->reputation_positive_delta = kbo_amateur_policy_int("reputation_positive_delta", 1, -100, 100);
    p->reputation_poor_score_max = kbo_amateur_policy_int("reputation_poor_score_max", 500, 0, 10000);
    p->reputation_poor_delta = kbo_amateur_policy_int("reputation_poor_delta", -4, -100, 100);
    p->reputation_weak_score_max = kbo_amateur_policy_int("reputation_weak_score_max", 700, 0, 10000);
    p->reputation_weak_delta = kbo_amateur_policy_int("reputation_weak_delta", -2, -100, 100);
    p->reputation_negative_score_max = kbo_amateur_policy_int("reputation_negative_score_max", 900, 0, 10000);
    p->reputation_negative_delta = kbo_amateur_policy_int("reputation_negative_delta", -1, -100, 100);
    p->reputation_top_major_rank_count = kbo_amateur_policy_int("reputation_top_major_rank_count", 4, 0, 1000);
    p->reputation_top_major_bonus = kbo_amateur_policy_int("reputation_top_major_bonus", 2, 0, 100);
    p->reputation_top_minor_rank_count = kbo_amateur_policy_int("reputation_top_minor_rank_count", 10, 0, 1000);
    p->reputation_top_minor_bonus = kbo_amateur_policy_int("reputation_top_minor_bonus", 1, 0, 100);
    p->reputation_bottom_major_rank_count = kbo_amateur_policy_int("reputation_bottom_major_rank_count", 4, 0, 1000);
    p->reputation_bottom_major_penalty = kbo_amateur_policy_int("reputation_bottom_major_penalty", 2, 0, 100);
    p->reputation_bottom_minor_rank_count = kbo_amateur_policy_int("reputation_bottom_minor_rank_count", 10, 0, 1000);
    p->reputation_bottom_minor_penalty = kbo_amateur_policy_int("reputation_bottom_minor_penalty", 1, 0, 100);
    p->ortools_batch_idle_ms = kbo_amateur_policy_int("ortools_batch_idle_ms", 10000, 0, 600000);
    p->ortools_batch_near_complete_idle_ms = kbo_amateur_policy_int("ortools_batch_near_complete_idle_ms", 1500, 0, 600000);
    p->ortools_batch_near_complete_teams = kbo_amateur_policy_int("ortools_batch_near_complete_teams", 96, 0, 1000000);
    p->ortools_batch_near_complete_players = kbo_amateur_policy_int("ortools_batch_near_complete_players", 3000, 0, 1000000);
    if (p->target_reputation_max < p->target_reputation_min) {
        p->target_reputation_max = p->target_reputation_min;
    }
    if (p->college_age_max < p->college_age_min) {
        p->college_age_max = p->college_age_min;
    }
    if (p->high_school_age_max < p->high_school_age_min) {
        p->high_school_age_max = p->high_school_age_min;
    }
    if (p->high_school_assignment_target_max_players < p->high_school_assignment_source_min_players) {
        p->high_school_assignment_target_max_players = p->high_school_assignment_source_min_players;
    }
    if (p->college_assignment_target_max_players < p->college_assignment_source_min_players) {
        p->college_assignment_target_max_players = p->college_assignment_source_min_players;
    }
    if (p->college_reputation_max < p->college_reputation_min) {
        p->college_reputation_max = p->college_reputation_min;
    }
    if (p->high_school_reputation_max < p->high_school_reputation_min) {
        p->high_school_reputation_max = p->high_school_reputation_min;
    }
    return TRUE;
}

const KboAmateurPlayerQualityPolicy* kbo_amateur_player_quality_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_amateur_player_quality_policy_once,
        kbo_amateur_player_quality_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_amateur_player_quality_policy;
}

uint8_t kbo_amateur_default_team_reputation(void)
{
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    return (uint8_t)policy->default_team_reputation;
}
