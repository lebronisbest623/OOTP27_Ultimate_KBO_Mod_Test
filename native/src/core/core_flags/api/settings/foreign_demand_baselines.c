#include "../flags_api.h"

#include "../../localappdata/localappdata_reader.h"

#include <stdint.h>

static const char* KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[9] = {
    "foreign_fa_demand_minimum_salary",
    "foreign_fa_demand_poor_salary",
    "foreign_fa_demand_fair_salary",
    "foreign_fa_demand_below_average_salary",
    "foreign_fa_demand_average_salary",
    "foreign_fa_demand_above_average_salary",
    "foreign_fa_demand_good_salary",
    "foreign_fa_demand_star_salary",
    "foreign_fa_demand_superstar_salary"
};

static const char* KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[9] = {
    "asian_quota_fa_demand_minimum_salary",
    "asian_quota_fa_demand_poor_salary",
    "asian_quota_fa_demand_fair_salary",
    "asian_quota_fa_demand_below_average_salary",
    "asian_quota_fa_demand_average_salary",
    "asian_quota_fa_demand_above_average_salary",
    "asian_quota_fa_demand_good_salary",
    "asian_quota_fa_demand_star_salary",
    "asian_quota_fa_demand_superstar_salary"
};

static const int32_t KBO_FOREIGN_FA_DEMAND_BASELINE_DEFAULTS[9] = {
    300000, 300000, 320000, 350000, 450000, 600000, 800000, 1000000, 1500000
};

static const int32_t KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_DEFAULTS[9] = {
    80000, 100000, 120000, 150000, 200000, 300000, 450000, 650000, 900000
};

#define KBO_ASIAN_QUOTA_SALARY_LIMIT_KEY "asian_quota_salary_limit"
#define KBO_ASIAN_QUOTA_SALARY_LIMIT_DEFAULT 200000

int32_t kbo_clamp_foreign_fa_demand_baseline_value(int32_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 20000000) {
        return 20000000;
    }
    return value;
}

int32_t kbo_get_foreign_fa_demand_baseline_value(int index)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    int value = KBO_FOREIGN_FA_DEMAND_BASELINE_DEFAULTS[index];
    if (!kbo_read_localappdata_json_int_value(KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[index], &value)) {
        value = KBO_FOREIGN_FA_DEMAND_BASELINE_DEFAULTS[index];
    }
    return kbo_clamp_foreign_fa_demand_baseline_value(value);
}

int32_t kbo_get_asian_quota_fa_demand_baseline_value(int index)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    int value = KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_DEFAULTS[index];
    if (!kbo_read_localappdata_json_int_value(KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[index], &value)) {
        value = KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_DEFAULTS[index];
    }
    return kbo_clamp_foreign_fa_demand_baseline_value(value);
}

int32_t kbo_get_foreign_fa_demand_baseline_value_for_player(int index, int asian_quota)
{
    return asian_quota
        ? kbo_get_asian_quota_fa_demand_baseline_value(index)
        : kbo_get_foreign_fa_demand_baseline_value(index);
}

int kbo_set_foreign_fa_demand_baseline_value(int index, int32_t value)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    return kbo_write_localappdata_json_int_value(
        KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[index],
        kbo_clamp_foreign_fa_demand_baseline_value(value));
}

int kbo_set_asian_quota_fa_demand_baseline_value(int index, int32_t value)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    return kbo_write_localappdata_json_int_value(
        KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[index],
        kbo_clamp_foreign_fa_demand_baseline_value(value));
}

int32_t kbo_clamp_asian_quota_salary_limit_value(int32_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 20000000) {
        return 20000000;
    }
    return value;
}

int32_t kbo_get_asian_quota_salary_limit(void)
{
    int value = KBO_ASIAN_QUOTA_SALARY_LIMIT_DEFAULT;
    if (!kbo_read_localappdata_json_int_value(KBO_ASIAN_QUOTA_SALARY_LIMIT_KEY, &value)) {
        value = KBO_ASIAN_QUOTA_SALARY_LIMIT_DEFAULT;
    }
    return kbo_clamp_asian_quota_salary_limit_value(value);
}

int kbo_set_asian_quota_salary_limit(int32_t value)
{
    return kbo_write_localappdata_json_int_value(
        KBO_ASIAN_QUOTA_SALARY_LIMIT_KEY,
        kbo_clamp_asian_quota_salary_limit_value(value));
}
