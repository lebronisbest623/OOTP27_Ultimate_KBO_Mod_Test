#include "../internal/foreign_quota_internal.h"

/* OOTP active foreign count wrappers. Included from native/KBOFix.c. */

__declspec(noinline) int32_t ootp_kbo_active_foreign_hitter_count_wrapper(
    uintptr_t team_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    typedef int32_t (__fastcall *OriginalFn)(uintptr_t);
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    int32_t original = ((OriginalFn)original_func_ptr)(team_ptr);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    int32_t result = original;
    if (kbo_custom_foreign_policy_enabled()) {
        result = kbo_custom_foreign_policy_neutralized_count(team_ptr, original, 0);
    } else {
        result = kbo_apply_active_asian_quota_count_exception(team_ptr, original, 0);
    }
    kbo_perf_probe_record(
        "active_foreign_hitter_count",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.active_hitter_count", result);
}

__declspec(noinline) int32_t ootp_kbo_active_foreign_pitcher_count_wrapper(
    uintptr_t team_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    typedef int32_t (__fastcall *OriginalFn)(uintptr_t);
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    int32_t original = ((OriginalFn)original_func_ptr)(team_ptr);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    int32_t result = original;
    if (kbo_custom_foreign_policy_enabled()) {
        result = kbo_custom_foreign_policy_neutralized_count(team_ptr, original, 1);
    } else {
        result = kbo_apply_active_asian_quota_count_exception(team_ptr, original, 1);
    }
    kbo_perf_probe_record(
        "active_foreign_pitcher_count",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.active_pitcher_count", result);
}

