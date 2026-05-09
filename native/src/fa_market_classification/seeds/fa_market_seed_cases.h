#ifndef KBOFIX_SRC_FA_MARKET_CLASSIFICATION_FA_MARKET_SEED_CASES_H_
#define KBOFIX_SRC_FA_MARKET_CLASSIFICATION_FA_MARKET_SEED_CASES_H_

#include <stdint.h>
#include <stddef.h>

struct KboFaMarketSeedCase;

const char* kbo_fa_market_canonical_case_label(const char* case_label);
int kbo_fa_market_case_is_seeded_official(const char* case_label);
int kbo_load_fa_market_seed_cases(
    struct KboFaMarketSeedCase* seeds,
    int max_seeds,
    char* out_path,
    size_t out_path_size);
const struct KboFaMarketSeedCase* kbo_find_fa_market_seed_case(
    const struct KboFaMarketSeedCase* seeds,
    int seed_count,
    uint32_t player_id);

#endif
