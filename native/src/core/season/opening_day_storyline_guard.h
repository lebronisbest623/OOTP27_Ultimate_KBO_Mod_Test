#ifndef KBOFIX_SRC_CORE_SEASON_OPENING_DAY_STORYLINE_GUARD_H_
#define KBOFIX_SRC_CORE_SEASON_OPENING_DAY_STORYLINE_GUARD_H_

#include <stdint.h>

int kbo_opening_day_storyline_guard_active(
    const char* source,
    uint32_t* out_date_key,
    uint32_t* out_opening_day);

#endif
