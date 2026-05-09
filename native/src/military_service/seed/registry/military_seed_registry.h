#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SEED_REGISTRY_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SEED_REGISTRY_H_

#include <stdint.h>
#include "../parse/military_service_seed_parse.h"

#define KBO_MILITARY_SERVICE_SEED_MAX 512

void kbo_ensure_military_service_seeds_loaded(void);
int kbo_snapshot_military_service_seeds(KboMilitaryServiceSeed* out, int max_count);
int kbo_military_original_team_from_seed(
    uint32_t player_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id,
    uint32_t* out_original_team_id,
    uint32_t* out_original_league_id);

#endif
