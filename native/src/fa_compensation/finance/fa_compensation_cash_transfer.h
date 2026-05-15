#ifndef KBOFIX_SRC_FA_COMPENSATION_FINANCE_FA_COMPENSATION_CASH_TRANSFER_H_
#define KBOFIX_SRC_FA_COMPENSATION_FINANCE_FA_COMPENSATION_CASH_TRANSFER_H_

#include <stdint.h>

#include "../state/fa_compensation_state.h"

int kbo_apply_fa_compensation_cash_transfer(
    const KboFaCompensationRecord* rec,
    uint32_t amount,
    uint32_t applied_yyyymmdd,
    const char* action,
    const char* source);

#endif
