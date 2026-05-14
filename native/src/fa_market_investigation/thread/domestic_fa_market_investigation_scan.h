#ifndef KBOFIX_SRC_FA_MARKET_INVESTIGATION_THREAD_DOMESTIC_FA_MARKET_INVESTIGATION_SCAN_H_
#define KBOFIX_SRC_FA_MARKET_INVESTIGATION_THREAD_DOMESTIC_FA_MARKET_INVESTIGATION_SCAN_H_

#include <stdint.h>
#include <stddef.h>

#include "../../fa_market_classification/api/fa_market_classification.h"

#define KBO_DOMESTIC_FA_INVESTIGATION_MAX 256

typedef struct KboDomesticFaInvestigationCandidate {
    KboFaMarketClassification row;
    int32_t value_score;
    int32_t overall;
    int32_t talent;
    int32_t ratings;
    int32_t career;
    uint32_t market_days;
    char blockers[160];
} KboDomesticFaInvestigationCandidate;

uint32_t kbo_domestic_fa_date_gap_days(uint32_t older_yyyymmdd, uint32_t newer_yyyymmdd);
uint32_t kbo_domestic_fa_history_date_from_reason(const char* reason);
int kbo_domestic_fa_case_is_market_relevant(const char* case_label);
int kbo_domestic_fa_case_is_official_or_probable(const char* case_label);
int kbo_domestic_fa_row_is_quality_candidate(
    const KboFaMarketClassification* row,
    int32_t value_score);
void kbo_domestic_fa_describe_blockers(
    const KboFaMarketClassification* row,
    int32_t value_score,
    uint32_t market_days,
    char* out,
    size_t out_size);
int kbo_domestic_fa_compare_candidates_desc(const void* lhs, const void* rhs);
int kbo_domestic_fa_write_investigation_csv(
    const KboDomesticFaInvestigationCandidate* candidates,
    int candidate_count,
    const KboFaMarketScanSummary* summary,
    char* out_path,
    size_t out_path_size);

#endif
