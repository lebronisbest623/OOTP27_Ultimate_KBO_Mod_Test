#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_DISPATCH_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_DISPATCH_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_dispatch_custom_event(uintptr_t event_ptr, const char* name, uint32_t event_yyyymmdd, uint32_t event_year, uint32_t event_month, uint32_t event_day, const char* source);

#endif
