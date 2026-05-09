#ifndef KBOFIX_SRC_ALLSTAR_ALLSTAR_NATIVE_EVENTS_SCHEDULE_DATES_H_
#define KBOFIX_SRC_ALLSTAR_ALLSTAR_NATIVE_EVENTS_SCHEDULE_DATES_H_

#include <stddef.h>
#include <stdint.h>

typedef struct KboAllstarScheduleDate {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} KboAllstarScheduleDate;

int kbo_allstar_schedule_date_ready(uint8_t* league);
int kbo_allstar_season_start_date_ready(uint8_t* league);
int seed_kbo_allstar_schedule_dates(uintptr_t league_ptr, const char* source);

#endif
