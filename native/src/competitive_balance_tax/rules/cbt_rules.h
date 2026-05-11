#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_RULES_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_RULES_H_

#include <stdint.h>

typedef struct KboCbtRules {
    uint8_t  enabled;
    uint32_t top_player_count;
    uint32_t draft_penalty_min_consecutive;
    uint32_t draft_penalty_stages;
    uint32_t tax_rate_1;
    uint32_t tax_rate_2;
    uint32_t tax_rate_3plus;
    uint32_t annual_increase_pct;
    int32_t  threshold_override;
} KboCbtRules;

void    kbo_cbt_rules_load(KboCbtRules* out);
int32_t kbo_cbt_get_threshold(const KboCbtRules* rules, uint32_t season);
uint32_t kbo_cbt_get_tax_rate(const KboCbtRules* rules, uint32_t consecutive_count);

#endif
