#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_EVENTS_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_EVENTS_H_

#include <stdint.h>

int kbo_open_foreign_waiver_window(uint32_t today_yyyymmdd, uint32_t today_serial, const char* reason);
int kbo_announce_foreign_waiver_results(uint32_t event_yyyymmdd, const char* source);

#endif
