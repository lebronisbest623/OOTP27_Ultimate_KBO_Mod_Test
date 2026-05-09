#ifndef KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_H_
#define KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_H_

#include <stdint.h>

void kbo_intl_established_fa_postscan_schedule(
    int32_t original_count,
    int32_t expected_count,
    int multiplier,
    uint32_t primary_league_id,
    uint32_t fallback_league_id);
void start_kbo_intl_established_fa_postscan_thread(void);
int kbo_intl_established_fa_pitcher_role_is_bullpen(uint8_t position_role);
const char* kbo_intl_established_fa_quality_policy_label(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role);
int32_t kbo_intl_established_fa_quality_score_cap(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role);

#endif
