#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_HISTORY_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_HISTORY_H_

#include <stdint.h>

int kbo_record_fa_compensation_signing(
    uintptr_t player_ptr,
    uint32_t signing_team_id,
    uint32_t league_id,
    const char* source);

const char* kbo_fa_compensation_status_label(uint8_t status);

#endif
