#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_WINDOW_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_WINDOW_H_

#include <stdint.h>

int kbo_handle_independent_team_acquisition_open_event(
    uint32_t event_yyyymmdd,
    const char* source);
uint32_t kbo_independent_team_acquisition_window_open_date(void);

#endif
