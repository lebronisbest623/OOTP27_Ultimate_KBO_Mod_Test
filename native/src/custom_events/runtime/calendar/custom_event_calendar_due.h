#ifndef KBOFIX_SRC_CUSTOM_EVENTS_RUNTIME_CALENDAR_CUSTOM_EVENT_CALENDAR_DUE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_RUNTIME_CALENDAR_CUSTOM_EVENT_CALENDAR_DUE_H_

#include <stdint.h>

int kbo_process_custom_events_due_through(uint32_t today_yyyymmdd, const char* source);

#endif
