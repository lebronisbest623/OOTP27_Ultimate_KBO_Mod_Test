#ifndef KBO_CORE_LEAGUE_EVENTS_H
#define KBO_CORE_LEAGUE_EVENTS_H

#include <stdint.h>

int create_kbo_league_event(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t event_type,
    const char* title,
    uint16_t aux_id,
    const char* source);

#endif
