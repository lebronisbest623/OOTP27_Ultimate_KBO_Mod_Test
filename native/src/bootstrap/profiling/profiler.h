#ifndef KBO_BOOTSTRAP_PROFILER_H
#define KBO_BOOTSTRAP_PROFILER_H

#include <windows.h>

int kbo_profiler_begin(LARGE_INTEGER* out_start);
void kbo_profiler_end(const char* name, const LARGE_INTEGER* start);
int kbo_profiler_is_enabled(void);
void kbo_profiler_record_us(const char* name, unsigned long long elapsed_us);
void kbo_profiler_reset_enabled_cache(void);

#define KBO_PROFILE_BEGIN(var_name) \
    LARGE_INTEGER var_name = {0}; \
    int var_name##_active = kbo_profiler_begin(&var_name)

#define KBO_PROFILE_END(var_name, zone_name) \
    do { \
        if (var_name##_active) { \
            kbo_profiler_end((zone_name), &var_name); \
        } \
    } while (0)

#endif
