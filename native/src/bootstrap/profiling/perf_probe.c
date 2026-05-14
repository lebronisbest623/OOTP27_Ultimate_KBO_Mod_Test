#include "perf_probe.h"

#include "../../core/logging/core_log.h"
#include "profiler.h"

void kbo_perf_probe_record(
    const char* name,
    volatile LONG* total_calls,
    volatile LONG* last_calls,
    volatile LONG* total_ms,
    volatile LONG* max_ms,
    volatile LONG* last_tick,
    DWORD elapsed_ms)
{
    if (elapsed_ms > 0u) {
        kbo_profiler_record_us(
            name != NULL ? name : "legacy_perf_probe",
            (unsigned long long)elapsed_ms * 1000ULL);
    } else if (kbo_profiler_is_enabled()) {
        kbo_profiler_record_us(
            name != NULL ? name : "legacy_perf_probe",
            0ULL);
    }

    LONG total = InterlockedIncrement(total_calls);
    if (elapsed_ms > 0u) {
        LONG elapsed = elapsed_ms > 0x7fffffffu ? 0x7fffffff : (LONG)elapsed_ms;
        InterlockedExchangeAdd(total_ms, elapsed);
        LONG old_max = *max_ms;
        while (elapsed > old_max
                && InterlockedCompareExchange(max_ms, elapsed, old_max) != old_max) {
            old_max = *max_ms;
        }
    }

    DWORD now = GetTickCount();
    LONG now_tick = (LONG)now;
    LONG previous_tick = *last_tick;
    if (previous_tick == 0) {
        if (InterlockedCompareExchange(last_tick, now_tick, 0) == 0) {
            return;
        }
        previous_tick = *last_tick;
    }

    DWORD window_ms = now - (DWORD)previous_tick;
    if (window_ms < 1000u) {
        return;
    }
    if (InterlockedCompareExchange(last_tick, now_tick, previous_tick) != previous_tick) {
        return;
    }

    LONG previous_calls = InterlockedExchange(last_calls, total);
    LONG delta_calls = total - previous_calls;
    LONG delta_ms = InterlockedExchange(total_ms, 0);
    LONG window_max_ms = InterlockedExchange(max_ms, 0);
    if (delta_calls >= 50 || delta_ms >= 20 || window_max_ms >= 10) {
        kbo_log_runtimef(
            "KBO perf hook=%s total=%ld delta=%ld elapsed_ms=%ld max_ms=%ld window_ms=%lu",
            name != NULL ? name : "",
            total,
            delta_calls,
            delta_ms,
            window_max_ms,
            (unsigned long)window_ms);
    }
}
