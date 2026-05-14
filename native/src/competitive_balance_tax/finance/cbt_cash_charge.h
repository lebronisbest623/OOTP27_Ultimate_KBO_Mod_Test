#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_FINANCE_CBT_CASH_CHARGE_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_FINANCE_CBT_CASH_CHARGE_H_

#include <stdint.h>

int kbo_cbt_apply_offseason_cash_charges(uint32_t season, uint32_t applied_yyyymmdd, const char* source);

#endif
