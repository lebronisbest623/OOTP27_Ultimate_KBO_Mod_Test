#include "custom_event_calendar_due.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../../competitive_balance_tax/events/cbt_events.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../asian_games/schedule/asian_games_schedule.h"
#include "../../schedules/independent/independent_team_acquisition_schedule.h"
#include "../../schedules/priority/foreign_priority_event_schedule.h"
#include "../scan/custom_event_scan.h"

static int kbo_custom_event_calendar_cursor_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("custom_event_calendar_cursor.txt", out, out_size);
}

static uint32_t kbo_custom_event_calendar_read_cursor(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_custom_event_calendar_cursor_path(path, sizeof(path))) {
        return 0u;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return 0u;
    }

    unsigned int value = 0u;
    int matched = fscanf(file, "%u", &value);
    fclose(file);
    return matched == 1 ? (uint32_t)value : 0u;
}

static void kbo_custom_event_calendar_write_cursor(uint32_t today_yyyymmdd, const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_custom_event_calendar_cursor_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "KBO custom event calendar cursor skipped source=%s reason=path_unavailable today=%u",
            source != NULL ? source : "",
            today_yyyymmdd);
        return;
    }

    FILE* file = fopen(path, "w");
    if (file == NULL) {
        kbo_log_runtimef(
            "KBO custom event calendar cursor skipped source=%s reason=open_failed today=%u path=%s",
            source != NULL ? source : "",
            today_yyyymmdd,
            path);
        return;
    }

    fprintf(file, "%u\n", today_yyyymmdd);
    fclose(file);
}

static int kbo_custom_event_calendar_scan_until_idle(const char* source)
{
    int total_triggered = 0;
    for (int attempt = 0; attempt < 32; attempt++) {
        int triggered = scan_kbo_custom_events_once(source);
        if (triggered < 0) {
            return total_triggered > 0 ? total_triggered : -1;
        }
        if (triggered == 0) {
            return total_triggered;
        }
        total_triggered += triggered;
    }

    kbo_log_runtimef(
        "KBO custom event calendar scan stopped source=%s reason=attempt_limit total_triggered=%d",
        source != NULL ? source : "",
        total_triggered);
    return total_triggered;
}

int kbo_process_custom_events_due_through(uint32_t today_yyyymmdd, const char* source)
{
    if (today_yyyymmdd == 0u) {
        return -1;
    }

    uint32_t previous_cursor = kbo_custom_event_calendar_read_cursor();
    if (previous_cursor >= today_yyyymmdd) {
        kbo_log_runtimef(
            "KBO custom event calendar due-through skipped source=%s previous_cursor=%u today=%u reason=cursor_current",
            source != NULL ? source : "",
            previous_cursor,
            today_yyyymmdd);
        return KBO_CUSTOM_EVENT_DUE_RESULT_NOOP;
    }

    int foreign_schedule = kbo_schedule_foreign_priority_custom_events(source);
    int asian_schedule = kbo_schedule_asian_games_custom_events(source);
    int cbt_schedule = kbo_schedule_cbt_custom_events(source);
    int independent_schedule = kbo_schedule_independent_team_acquisition_custom_events(source);
    int scanned = kbo_custom_event_calendar_scan_until_idle(source);

    int schedule_blocked = foreign_schedule < 0
        && asian_schedule < 0
        && cbt_schedule < 0
        && independent_schedule < 0;
    int deferred = schedule_blocked || scanned < 0;
    if (!deferred) {
        kbo_custom_event_calendar_write_cursor(today_yyyymmdd, source);
    }

    kbo_log_runtimef(
        "KBO custom event calendar due-through source=%s previous_cursor=%u today=%u foreign=%d asian=%d cbt=%d independent=%d scanned=%d schedule_blocked=%d deferred=%d",
        source != NULL ? source : "",
        previous_cursor,
        today_yyyymmdd,
        foreign_schedule,
        asian_schedule,
        cbt_schedule,
        independent_schedule,
        scanned,
        schedule_blocked,
        deferred);

    if (deferred) {
        return -1;
    }
    return (foreign_schedule > 0 || asian_schedule > 0 || cbt_schedule > 0 || independent_schedule > 0 || scanned > 0)
        ? KBO_CUSTOM_EVENT_DUE_RESULT_CHANGED
        : KBO_CUSTOM_EVENT_DUE_RESULT_SCANNED_IDLE;
}
