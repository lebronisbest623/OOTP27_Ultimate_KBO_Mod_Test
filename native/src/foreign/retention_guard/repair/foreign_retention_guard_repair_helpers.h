#ifndef KBOFIX_SRC_FOREIGN_RETENTION_GUARD_REPAIR_HELPERS_H_
#define KBOFIX_SRC_FOREIGN_RETENTION_GUARD_REPAIR_HELPERS_H_

#include <stdint.h>

#include "../foreign_retention_guard_internal.h"

int kbo_foreign_retention_guard_stamp_signed_state(
    uint8_t* player,
    uint8_t* team,
    const KboForeignRetentionGuardRecord* rec);
int kbo_foreign_retention_guard_clean_signed_holder(
    uint8_t* player,
    uint8_t* team,
    const KboForeignRetentionGuardRecord* rec);
int kbo_foreign_retention_guard_player_repairable(
    uint8_t* player,
    const KboForeignRetentionGuardRecord* rec);
void kbo_foreign_retention_guard_log_cleaned(
    const char* source,
    const KboForeignRetentionGuardRecord* rec,
    uint8_t* player,
    uint32_t today,
    uint32_t before_current,
    uint32_t before_active,
    uint32_t before_original,
    uint32_t before_default,
    uint32_t before_league,
    uint8_t before_restricted,
    uint8_t before_secondary,
    uint8_t before_contract_level);
void kbo_foreign_retention_guard_log_restored(
    const char* source,
    const KboForeignRetentionGuardRecord* rec,
    uint8_t* player,
    uint32_t today,
    uint32_t before_current,
    uint32_t before_active,
    uint32_t before_original,
    uint32_t before_default,
    uint32_t before_league,
    uint8_t before_restricted,
    uint8_t before_secondary,
    uint8_t before_contract_level);

#endif
