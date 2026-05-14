#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "captain_selection_policy.h"

#include "../../core/core_flags/localappdata/localappdata_reader.h"

#define KBO_CAPTAIN_SELECTION_POLICY_FILE "captain_selection_policy.json"

static INIT_ONCE g_kbo_captain_selection_policy_once = INIT_ONCE_STATIC_INIT;
static KboCaptainSelectionPolicy g_kbo_captain_selection_policy;

static int32_t kbo_captain_selection_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    int value = (int)fallback;
    if (kbo_read_localappdata_named_json_int_value(KBO_CAPTAIN_SELECTION_POLICY_FILE, key, &value)
            && value >= min_value && value <= max_value) {
        return (int32_t)value;
    }
    return fallback;
}

static BOOL CALLBACK kbo_captain_selection_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboCaptainSelectionPolicy* p = &g_kbo_captain_selection_policy;
    p->eligible_age_min = kbo_captain_selection_policy_int("eligible_age_min", 18, 0, 80);
    p->eligible_age_max = kbo_captain_selection_policy_int("eligible_age_max", 48, 0, 80);
    p->salary_score_divisor = kbo_captain_selection_policy_int("salary_score_divisor", 1000, 1, 100000000);
    p->salary_score_max = kbo_captain_selection_policy_int("salary_score_max", 120000, 0, 10000000);
    p->age_core_min = kbo_captain_selection_policy_int("age_core_min", 30, 0, 80);
    p->age_core_max = kbo_captain_selection_policy_int("age_core_max", 36, 0, 80);
    p->age_core_score = kbo_captain_selection_policy_int("age_core_score", 60000, 0, 10000000);
    p->age_extended_min = kbo_captain_selection_policy_int("age_extended_min", 28, 0, 80);
    p->age_extended_max = kbo_captain_selection_policy_int("age_extended_max", 39, 0, 80);
    p->age_extended_score = kbo_captain_selection_policy_int("age_extended_score", 42000, 0, 10000000);
    p->age_depth_min = kbo_captain_selection_policy_int("age_depth_min", 25, 0, 80);
    p->age_depth_max = kbo_captain_selection_policy_int("age_depth_max", 42, 0, 80);
    p->age_depth_score = kbo_captain_selection_policy_int("age_depth_score", 18000, 0, 10000000);
    p->domestic_bonus = kbo_captain_selection_policy_int("domestic_bonus", 70000, 0, 10000000);
    p->foreign_penalty = kbo_captain_selection_policy_int("foreign_penalty", 180000, 0, 10000000);
    p->active_team_bonus = kbo_captain_selection_policy_int("active_team_bonus", 30000, 0, 10000000);
    p->current_team_bonus = kbo_captain_selection_policy_int("current_team_bonus", 15000, 0, 10000000);
    p->dfa_penalty = kbo_captain_selection_policy_int("dfa_penalty", 140000, 0, 10000000);
    p->restricted_penalty = kbo_captain_selection_policy_int("restricted_penalty", 50000, 0, 10000000);
    p->injured_penalty = kbo_captain_selection_policy_int("injured_penalty", 12000, 0, 10000000);
    if (p->eligible_age_max < p->eligible_age_min) {
        p->eligible_age_max = p->eligible_age_min;
    }
    return TRUE;
}

const KboCaptainSelectionPolicy* kbo_captain_selection_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_captain_selection_policy_once,
        kbo_captain_selection_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_captain_selection_policy;
}
