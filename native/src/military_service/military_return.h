#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_RETURN_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_RETURN_H_

#include <stdint.h>

int kbo_return_completed_military_loan_player(
    uint8_t* player,
    const char* source,
    uint32_t vector_offset,
    int require_registered);

#endif
