#ifndef KBOFIX_SRC_FA_REQUALIFICATION_POLICY_H_
#define KBOFIX_SRC_FA_REQUALIFICATION_POLICY_H_

#include <stdint.h>

typedef struct KboFaRequalificationPolicy {
    int32_t team_control_years;
} KboFaRequalificationPolicy;

const KboFaRequalificationPolicy* kbo_fa_requalification_policy(void);
uint32_t kbo_fa_requalification_team_control_years(void);

#endif
