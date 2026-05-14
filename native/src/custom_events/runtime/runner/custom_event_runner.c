#include "custom_event_runner.h"

#include <stdio.h>

#include "../../../core/logging/core_log.h"
#include "../dispatch/custom_event_dispatch.h"
#include "../ledger/custom_event_ledger.h"
#include "../markers/custom_event_markers.h"

int kbo_run_custom_event_by_kind(
    uintptr_t event_ptr,
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* title,
    const char* source)
{
    if (event_yyyymmdd == 0u
            || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN
            || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return -1;
    }

    if (kbo_custom_event_ledger_completed(league_id, event_yyyymmdd, kind)
            || kbo_custom_event_processed_marker_exists_for_kind(event_yyyymmdd, kind)) {
        if (event_ptr != 0) {
            kbo_mark_custom_event_processed(event_ptr);
        }
        if (title != NULL && title[0] != '\0') {
            kbo_persist_custom_event_processed_marker(event_yyyymmdd, title, source);
        }
        append_logf(
            "KBO custom event runner skipped completed source=%s kind=%s date=%u league_id=%u",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_yyyymmdd,
            league_id);
        return KBO_CUSTOM_EVENT_RUN_ALREADY_COMPLETED;
    }

    uint32_t event_year = event_yyyymmdd / 10000u;
    uint32_t event_month = (event_yyyymmdd / 100u) % 100u;
    uint32_t event_day = event_yyyymmdd % 100u;
    int result = kbo_dispatch_custom_event_by_kind(
        event_ptr,
        kind,
        event_yyyymmdd,
        event_year,
        event_month,
        event_day,
        source);

    if (result > 0) {
        if (event_ptr != 0) {
            kbo_mark_custom_event_processed(event_ptr);
        }
        if (title != NULL && title[0] != '\0') {
            kbo_persist_custom_event_processed_marker(event_yyyymmdd, title, source);
        }
        kbo_custom_event_ledger_record(
            league_id,
            event_yyyymmdd,
            kind,
            "completed",
            result,
            title,
            "handler_completed",
            source);
        return result;
    }

    if (result == 0) {
        kbo_custom_event_ledger_record(
            league_id,
            event_yyyymmdd,
            kind,
            "deferred",
            result,
            title,
            "handler_deferred",
            source);
        return 0;
    }

    kbo_custom_event_ledger_record(
        league_id,
        event_yyyymmdd,
        kind,
        "failed",
        result,
        title,
        "handler_failed",
        source);
    return -1;
}
