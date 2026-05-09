#include "../flags_api.h"

#include "../../localappdata/localappdata_reader.h"

#define KBO_INTL_ESTABLISHED_FA_MULTIPLIER_KEY "intl_established_fa_multiplier"
#define KBO_INTL_ESTABLISHED_FA_MULTIPLIER_DEFAULT 20
#define KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MIN 1
#define KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MAX 20

int kbo_clamp_intl_established_fa_multiplier(int value)
{
    if (value < KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MIN) {
        return KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MIN;
    }
    if (value > KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MAX) {
        return KBO_INTL_ESTABLISHED_FA_MULTIPLIER_MAX;
    }
    return value;
}

int kbo_get_intl_established_fa_multiplier(void)
{
    int value = KBO_INTL_ESTABLISHED_FA_MULTIPLIER_DEFAULT;
    if (!kbo_read_localappdata_json_int_value(KBO_INTL_ESTABLISHED_FA_MULTIPLIER_KEY, &value)) {
        value = KBO_INTL_ESTABLISHED_FA_MULTIPLIER_DEFAULT;
    }
    return kbo_clamp_intl_established_fa_multiplier(value);
}

int kbo_set_intl_established_fa_multiplier(int value)
{
    return kbo_write_localappdata_json_int_value(
        KBO_INTL_ESTABLISHED_FA_MULTIPLIER_KEY,
        kbo_clamp_intl_established_fa_multiplier(value));
}
