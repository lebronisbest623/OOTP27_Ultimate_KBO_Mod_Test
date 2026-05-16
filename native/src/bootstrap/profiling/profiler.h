#ifndef KBO_BOOTSTRAP_PROFILER_H
#define KBO_BOOTSTRAP_PROFILER_H

#include <windows.h>

int kbo_profiler_begin(LARGE_INTEGER* out_start);
void kbo_profiler_end(const char* name, const LARGE_INTEGER* start);
int kbo_profiler_is_enabled(void);
void kbo_profiler_record_us(const char* name, unsigned long long elapsed_us);
void kbo_profiler_reset_enabled_cache(void);

typedef struct KboHookProfileScope {
    LARGE_INTEGER segment_start;
    unsigned long long overhead_us;
    int active;
    int segment_active;
} KboHookProfileScope;

void kbo_hook_profile_begin(KboHookProfileScope* scope);
void kbo_hook_profile_pause(KboHookProfileScope* scope);
void kbo_hook_profile_resume(KboHookProfileScope* scope);
void kbo_hook_profile_end(const char* hook_name, KboHookProfileScope* scope);

#define KBO_PROFILE_BEGIN(var_name) \
    LARGE_INTEGER var_name = {0}; \
    int var_name##_active = kbo_profiler_begin(&var_name)

#define KBO_PROFILE_END(var_name, zone_name) \
    do { \
        if (var_name##_active) { \
            kbo_profiler_end((zone_name), &var_name); \
        } \
    } while (0)

#define KBO_HOOK_PROFILE_BEGIN(var_name) \
    KboHookProfileScope var_name = {0}; \
    kbo_hook_profile_begin(&var_name)

#define KBO_HOOK_PROFILE_PAUSE(var_name) \
    kbo_hook_profile_pause(&var_name)

#define KBO_HOOK_PROFILE_RESUME(var_name) \
    kbo_hook_profile_resume(&var_name)

#define KBO_HOOK_PROFILE_END(var_name, hook_name) \
    kbo_hook_profile_end((hook_name), &var_name)

#define KBO_HOOK_PROFILE_RETURN(var_name, hook_name, value) \
    do { \
        kbo_hook_profile_end((hook_name), &var_name); \
        return (value); \
    } while (0)

#define KBO_HOOK_PROFILE_RETURN_VOID(var_name, hook_name) \
    do { \
        kbo_hook_profile_end((hook_name), &var_name); \
        return; \
    } while (0)

#endif
