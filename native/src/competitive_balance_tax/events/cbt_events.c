#include "cbt_events.h"

#include <stdio.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/events/core_league_events.h"
#include "../../core/logging/core_log.h"
#include "../../custom_events/runtime/lookup/custom_event_lookup.h"
#include "../../custom_events/runtime/state/custom_event_state.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../hotkey_window/api/hotkey_window_refresh.h"
#include "../api/competitive_balance_tax.h"
#include "../exceptions/cbt_exceptions.h"

static volatile LONG g_kbo_cbt_event_scheduler_started = 0;

int kbo_schedule_cbt_custom_events(const char* source)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        append_logf("KBO CBT event schedule skipped source=%s reason=current_date_unavailable", source != NULL ? source : "");
        return -1;
    }

    uint32_t today = year * 10000u + month * 100u + day;
    uint32_t opening_day = 0u;
    if (!kbo_cbt_exception_resolve_opening_day(year, &opening_day)) {
        static uint32_t last_logged_no_opening_day = 0u;
        if (last_logged_no_opening_day != today) {
            last_logged_no_opening_day = today;
            append_logf(
                "KBO CBT event schedule skipped source=%s reason=opening_day_unavailable season=%u today=%u",
                source != NULL ? source : "",
                year,
                today);
        }
        return -1;
    }

    uint32_t deadline = kbo_add_days_yyyymmdd(opening_day, 6u);
    uint32_t announcement = kbo_add_days_yyyymmdd(opening_day, 7u);
    if (deadline == 0u || announcement == 0u || today > announcement) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        append_logf("KBO CBT event schedule skipped source=%s reason=league_id_unavailable season=%u", source != NULL ? source : "", year);
        return -1;
    }

    int deadline_past = deadline < today;
    int announcement_past = announcement < today;
    int deadline_exists = deadline_past || kbo_custom_event_exists_by_title_for_date(
        league_id,
        deadline,
        g_kbo_cbt_exception_deadline_event_title);
    int announcement_exists = announcement_past || kbo_custom_event_exists_by_title_for_date(
        league_id,
        announcement,
        g_kbo_cbt_announcement_event_title);

    int created_deadline = 0;
    if (!deadline_exists) {
        created_deadline = create_kbo_league_event(
            deadline / 10000u,
            (deadline / 100u) % 100u,
            deadline % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_cbt_exception_deadline_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_announcement = 0;
    if (!announcement_exists) {
        created_announcement = create_kbo_league_event(
            announcement / 10000u,
            (announcement / 100u) % 100u,
            announcement % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_cbt_announcement_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    deadline_exists = deadline_past || created_deadline || kbo_custom_event_exists_by_title_for_date(
        league_id,
        deadline,
        g_kbo_cbt_exception_deadline_event_title);
    announcement_exists = announcement_past || created_announcement || kbo_custom_event_exists_by_title_for_date(
        league_id,
        announcement,
        g_kbo_cbt_announcement_event_title);

    append_logf(
        "KBO CBT event schedule source=%s season=%u opening_day=%u deadline=%u announcement=%u created_deadline=%d created_announcement=%d ready=%d",
        source != NULL ? source : "",
        year,
        opening_day,
        deadline,
        announcement,
        created_deadline,
        created_announcement,
        deadline_exists && announcement_exists);
    return (deadline_exists && announcement_exists) ? (created_deadline || created_announcement) : -1;
}

static DWORD WINAPI kbo_cbt_event_scheduler_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO CBT event scheduler started");
    uint32_t last_attempt_date = 0u;
    for (int attempt = 1; attempt <= 180 && kbo_runtime_threads_should_continue(); attempt++) {
        uint32_t today = 0u;
        kbo_get_current_yyyymmdd(&today);
        int result = kbo_schedule_cbt_custom_events("cbt_early_event_scheduler");
        if (result >= 0) {
            append_logf(
                "KBO CBT event scheduler ready attempt=%d today=%u result=%d",
                attempt,
                today,
                result);
            break;
        }
        if (attempt == 1 || today != last_attempt_date || attempt == 10 || attempt == 30 || attempt == 60 || attempt == 120) {
            append_logf(
                "KBO CBT event scheduler waiting attempt=%d today=%u result=%d",
                attempt,
                today,
                result);
            last_attempt_date = today;
        }
        if (!kbo_runtime_sleep_should_continue(2000u)) {
            break;
        }
    }
    InterlockedExchange(&g_kbo_cbt_event_scheduler_started, 0);
    append_log_line("KBO CBT event scheduler stopped");
    return 0;
}

void start_kbo_cbt_event_scheduler_thread(void)
{
    if (!kbo_fix_enabled()) {
        append_log_line("KBO CBT event scheduler skipped reason=fix_disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_cbt_event_scheduler_started, 1, 0) != 0) {
        return;
    }
    HANDLE thread = CreateThread(NULL, 0, kbo_cbt_event_scheduler_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "CBT event scheduler");
    } else {
        InterlockedExchange(&g_kbo_cbt_event_scheduler_started, 0);
        append_logf("KBO CBT event scheduler failed to start gle=%lu", GetLastError());
    }
}

int kbo_handle_cbt_deadline_event(uint32_t event_yyyymmdd, const char* source)
{
    uint32_t season = event_yyyymmdd / 10000u;
    kbo_cbt_exception_auto_designate_missing(season, "cbt_deadline_event");
    append_logf(
        "KBO CBT exception designation deadline reached source=%s date=%u",
        source != NULL ? source : "",
        event_yyyymmdd);
    kbo_request_hotkey_window_refresh("cbt_exception_deadline");
    return 1;
}

int kbo_handle_cbt_announcement_event(uint32_t event_yyyymmdd, const char* source)
{
    uint32_t season = event_yyyymmdd / 10000u;
    kbo_cbt_exception_auto_designate_missing(season, "cbt_announcement_event");
    append_logf(
        "KBO CBT announcement event reached source=%s date=%u season=%u",
        source != NULL ? source : "",
        event_yyyymmdd,
        season);
    kbo_process_competitive_balance_tax_for_date(season, event_yyyymmdd, "cbt_announcement_event");
    return 1;
}
