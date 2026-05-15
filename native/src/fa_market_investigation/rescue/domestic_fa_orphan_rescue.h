#ifndef KBOFIX_SRC_FA_MARKET_INVESTIGATION_RESCUE_DOMESTIC_FA_ORPHAN_RESCUE_H_
#define KBOFIX_SRC_FA_MARKET_INVESTIGATION_RESCUE_DOMESTIC_FA_ORPHAN_RESCUE_H_

#include <stdint.h>

#include "../thread/domestic_fa_market_investigation_scan.h"

#define KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX 16

typedef struct KboDomesticFaOrphanRescueCachedCandidate {
    uint32_t player_id;
    uint32_t today;
    uint32_t market_days;
    uint16_t age;
    int32_t value_score;
    int32_t fa_demand;
    char grade[12];
    char case_label[48];
} KboDomesticFaOrphanRescueCachedCandidate;

int kbo_domestic_fa_orphan_rescue_enabled(void);
int kbo_domestic_fa_orphan_rescue_dry_run(void);
void kbo_domestic_fa_orphan_rescue_update_cache(
    uint32_t today,
    const KboDomesticFaInvestigationCandidate* candidates,
    int candidate_count);
int kbo_domestic_fa_orphan_rescue_collect_cached(
    uint32_t today,
    KboDomesticFaOrphanRescueCachedCandidate* out_candidates,
    int max_candidates);
int32_t kbo_domestic_fa_orphan_rescue_force_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index,
    uint32_t today);

#endif
