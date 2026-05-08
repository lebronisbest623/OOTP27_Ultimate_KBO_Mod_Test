#ifndef KBOFIX_SRC_CORE_CORE_CURRENT_DATE_H_
#define KBOFIX_SRC_CORE_CORE_CURRENT_DATE_H_

#include <stdint.h>

int kbo_current_date_is_valid(uint32_t* out_year, uint32_t* out_month, uint32_t* out_day);

#endif
