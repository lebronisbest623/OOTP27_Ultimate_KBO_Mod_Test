#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#include "fa_requalification_policy.h"
#include "../../core/policy/core_policy.h"

#define KBO_FA_REQUALIFICATION_POLICY_FILE "fa_requalification_policy.json"

static INIT_ONCE g_kbo_fa_requalification_policy_once = INIT_ONCE_STATIC_INIT;
static KboFaRequalificationPolicy g_kbo_fa_requalification_policy;

static int32_t kbo_fa_requalification_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    return kbo_read_clamped_policy_int(KBO_FA_REQUALIFICATION_POLICY_FILE, key, fallback, min_value, max_value);
}

static BOOL CALLBACK kbo_fa_requalification_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboFaRequalificationPolicy* p = &g_kbo_fa_requalification_policy;
    memset(p, 0, sizeof(*p));
    p->team_control_years = kbo_fa_requalification_policy_int("team_control_years", 4, 0, 20);
    return TRUE;
}

const KboFaRequalificationPolicy* kbo_fa_requalification_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_fa_requalification_policy_once,
        kbo_fa_requalification_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_fa_requalification_policy;
}

uint32_t kbo_fa_requalification_team_control_years(void)
{
    const KboFaRequalificationPolicy* policy = kbo_fa_requalification_policy();
    return policy->team_control_years > 0 ? (uint32_t)policy->team_control_years : 0u;
}
