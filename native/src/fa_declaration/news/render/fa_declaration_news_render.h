#ifndef KBOFIX_SRC_FA_DECLARATION_NEWS_RENDER_FA_DECLARATION_NEWS_RENDER_H_
#define KBOFIX_SRC_FA_DECLARATION_NEWS_RENDER_FA_DECLARATION_NEWS_RENDER_H_

#include <stddef.h>
#include <stdint.h>

#include "../fa_declaration_news.h"

const KboFaDeclarationCandidate* kbo_fa_declaration_news_find_best_candidate(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_filter,
    int retry_filter,
    const uint32_t* used_ids,
    int used_count);
void kbo_fa_declaration_copy_team_name(uint32_t team_id, char* out, size_t out_size);
void kbo_fa_declaration_copy_team_link(uint32_t team_id, char* out, size_t out_size);
void kbo_fa_declaration_news_build_candidate_list(
    char* out,
    size_t out_size,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_filter,
    int retry_filter,
    int limit,
    const char* template_key,
    const char* source);

#endif
