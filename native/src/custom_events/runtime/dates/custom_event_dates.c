#include "custom_event_dates.h"

#include "../../../core/dates/core_current_date.h"

uint32_t kbo_custom_event_effective_news_date(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return 0u;
    }

    uint32_t current_year = 0u;
    uint32_t current_month = 0u;
    uint32_t current_day = 0u;
    if (!kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        return event_yyyymmdd;
    }

    uint32_t current_yyyymmdd = current_year * 10000u + current_month * 100u + current_day;
    return current_yyyymmdd > event_yyyymmdd ? current_yyyymmdd : event_yyyymmdd;
}
