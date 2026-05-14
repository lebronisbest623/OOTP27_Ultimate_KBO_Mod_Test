#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_window.h"

#include <stdint.h>

#include "../../core/logging/core_log.h"

static volatile LONG g_kbo_independent_team_acquisition_open_date = 0;

int kbo_handle_independent_team_acquisition_open_event(
    uint32_t event_yyyymmdd,
    const char* source)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    LONG previous = InterlockedExchange(
        &g_kbo_independent_team_acquisition_open_date,
        (LONG)event_yyyymmdd);
    kbo_log_runtimef(
        "KBO independent futures acquisition window opened source=%s date=%u previous=%u",
        source != NULL ? source : "",
        event_yyyymmdd,
        (uint32_t)previous);
    return 1;
}

uint32_t kbo_independent_team_acquisition_window_open_date(void)
{
    return (uint32_t)InterlockedCompareExchange(
        &g_kbo_independent_team_acquisition_open_date,
        0,
        0);
}
