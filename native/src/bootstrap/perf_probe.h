#ifndef KBOFIX_SRC_BOOTSTRAP_PERF_PROBE_H_
#define KBOFIX_SRC_BOOTSTRAP_PERF_PROBE_H_

#include <windows.h>

void kbo_perf_probe_record(
    const char* name,
    volatile LONG* total_calls,
    volatile LONG* last_calls,
    volatile LONG* total_ms,
    volatile LONG* max_ms,
    volatile LONG* last_tick,
    DWORD elapsed_ms);

#endif
