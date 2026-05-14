#ifndef KBOFIX_SRC_FOREIGN_RETENTION_GUARD_INTERNAL_H_
#define KBOFIX_SRC_FOREIGN_RETENTION_GUARD_INTERNAL_H_

#include <stdint.h>

#include "foreign_retention_guard.h"
#include "../../bootstrap/abi/ootp_offsets.h"

#define KBO_FOREIGN_RETENTION_GUARD_MAX 128

typedef struct KboForeignRetentionGuardRecord {
    uint32_t player_id;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t signed_on_yyyymmdd;
    uint32_t expires_on_yyyymmdd;
    uint32_t last_repaired_on_yyyymmdd;
    uint32_t contract_status;
    uint32_t contract_start_year;
    int32_t contract_years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS];
    uint8_t repair_count;
    uint8_t active;
} KboForeignRetentionGuardRecord;

void kbo_foreign_retention_guard_snapshot(
    KboForeignRetentionGuardRecord* out_records,
    int* out_count,
    uint32_t today);

void kbo_foreign_retention_guard_mark_repaired(
    uint32_t player_id,
    uint32_t team_id,
    uint32_t today);

#endif
