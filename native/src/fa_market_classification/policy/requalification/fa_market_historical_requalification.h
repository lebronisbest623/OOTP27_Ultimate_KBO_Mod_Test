#ifndef KBOFIX_SRC_FA_MARKET_CLASSIFICATION_POLICY_FA_MARKET_HISTORICAL_REQUALIFICATION_H_
#define KBOFIX_SRC_FA_MARKET_CLASSIFICATION_POLICY_FA_MARKET_HISTORICAL_REQUALIFICATION_H_

#include <stdint.h>

#include "../../api/fa_market_classification.h"

int kbo_fa_market_apply_requalification_grade_override(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grade,
    const KboFaRequalificationRecord* requalification_records,
    int requalification_count,
    uint32_t current_year,
    const KboFaRules* rules);

#endif
