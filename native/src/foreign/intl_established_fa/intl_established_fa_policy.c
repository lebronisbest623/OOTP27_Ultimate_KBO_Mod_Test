#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "intl_established_fa_policy.h"

#include "../../core/core_flags/localappdata/localappdata_reader.h"

#define KBO_INTL_ESTABLISHED_FA_POLICY_FILE "intl_established_fa_policy.json"

static INIT_ONCE g_kbo_intl_established_fa_policy_once = INIT_ONCE_STATIC_INIT;
static KboIntlEstablishedFaPolicy g_kbo_intl_established_fa_policy;

static int32_t kbo_intl_established_fa_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    int value = (int)fallback;
    if (kbo_read_localappdata_named_json_int_value(KBO_INTL_ESTABLISHED_FA_POLICY_FILE, key, &value)
            && value >= min_value && value <= max_value) {
        return (int32_t)value;
    }
    return fallback;
}

static BOOL CALLBACK kbo_intl_established_fa_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboIntlEstablishedFaPolicy* p = &g_kbo_intl_established_fa_policy;
    p->max_scaled_count = kbo_intl_established_fa_policy_int("max_scaled_count", 1000, 1, 1000000);
    p->catcher_allow_one_in = kbo_intl_established_fa_policy_int("catcher_allow_one_in", 250, 1, 1000000);
    p->market_age_min = kbo_intl_established_fa_policy_int("market_age_min", 16, 0, 80);
    p->market_age_max = kbo_intl_established_fa_policy_int("market_age_max", 60, 0, 80);
    if (p->market_age_max < p->market_age_min) {
        p->market_age_max = p->market_age_min;
    }
    return TRUE;
}

const KboIntlEstablishedFaPolicy* kbo_intl_established_fa_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_intl_established_fa_policy_once,
        kbo_intl_established_fa_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_intl_established_fa_policy;
}
