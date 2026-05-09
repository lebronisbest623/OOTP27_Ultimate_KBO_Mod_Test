#ifndef KBOFIX_SRC_ALLSTAR_ALLSTAR_NATIVE_EVENTS_NATIVE_EVENT_LOGGING_H_
#define KBOFIX_SRC_ALLSTAR_ALLSTAR_NATIVE_EVENTS_NATIVE_EVENT_LOGGING_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void log_kbo_allstar_native_event_state(const char* prefix, uintptr_t league_ptr, const char* source);

#endif
