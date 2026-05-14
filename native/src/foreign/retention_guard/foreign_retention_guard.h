#ifndef KBOFIX_SRC_FOREIGN_RETENTION_GUARD_H_
#define KBOFIX_SRC_FOREIGN_RETENTION_GUARD_H_

#include <stdint.h>

void kbo_foreign_retention_guard_record_team_add(
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    uint32_t signed_on_yyyymmdd,
    uint32_t contract_status,
    uint32_t contract_start_year,
    const int32_t* contract_years,
    uint32_t contract_year_count);

void kbo_foreign_retention_guard_repair(const char* source);
int kbo_foreign_retention_guard_restore_recorded_holder_signing(
    const char* source,
    uint8_t* player,
    uint8_t* team,
    uint32_t today);

#endif
