#ifndef KBOFIX_SRC_FA_DECLARATION_NEWS_FA_DECLARATION_NEWS_H_
#define KBOFIX_SRC_FA_DECLARATION_NEWS_FA_DECLARATION_NEWS_H_

#include <stdint.h>

#include "../fa_declaration_internal.h"

int kbo_emit_fa_declaration_retry_news(
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int deferred_retry,
    const char* source);

int kbo_emit_fa_declaration_summary_news(
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_count,
    int deferred_count,
    int deferred_retry_count,
    int deferred_no_market_count,
    const char* source);

#endif
