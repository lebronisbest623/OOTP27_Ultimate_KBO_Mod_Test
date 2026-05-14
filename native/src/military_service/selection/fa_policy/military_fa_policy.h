#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_FA_POLICY_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_FA_POLICY_H_

#include <stdint.h>

uint32_t kbo_military_policy_current_yyyymmdd(void);
void kbo_record_recent_military_fa_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today);

#endif
