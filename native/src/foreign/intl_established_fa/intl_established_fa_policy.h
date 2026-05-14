#ifndef KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POLICY_H_
#define KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POLICY_H_

#include <stdint.h>

typedef struct KboIntlEstablishedFaPolicy {
    int32_t max_scaled_count;
    int32_t catcher_allow_one_in;
    int32_t market_age_min;
    int32_t market_age_max;
} KboIntlEstablishedFaPolicy;

const KboIntlEstablishedFaPolicy* kbo_intl_established_fa_policy(void);

#endif
