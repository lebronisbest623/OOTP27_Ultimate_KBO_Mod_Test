#ifndef KBOFIX_SRC_FA_DECLARATION_DECISION_SCORE_H_
#define KBOFIX_SRC_FA_DECLARATION_DECISION_SCORE_H_

#include "../../fa_declaration_internal.h"

int32_t kbo_fa_declaration_current_market_score(const KboFaDeclarationCandidate* candidate);
int32_t kbo_fa_declaration_upside_score(const KboFaDeclarationCandidate* candidate);
int kbo_fa_declaration_grade_rank(const char* grade);
int32_t kbo_fa_declaration_age_threshold_adjustment(uint16_t age);
int32_t kbo_fa_declaration_market_score(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score);
int kbo_fa_declaration_elite_market_fit(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int32_t market_score,
    int grade_rank);
int kbo_fa_declaration_should_retry_after_down_year(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int32_t form_gap,
    int grade_rank);
int kbo_fa_declaration_should_stay_no_market(
    const KboFaDeclarationCandidate* candidate,
    int32_t current_score,
    int32_t upside_score,
    int grade_rank,
    int elite_market_fit);

#endif