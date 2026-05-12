#include "../common/custom_events_common.h"
#include "custom_event_dispatch.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../names/custom_event_names.h"

int kbo_dispatch_custom_event(
    uintptr_t event_ptr,
    const char* name,
    uint32_t event_yyyymmdd,
    uint32_t event_year,
    uint32_t event_month,
    uint32_t event_day,
    const char* source)
{
    int is_open_event = kbo_custom_event_name_is_open(name);
    int is_close_event = kbo_custom_event_name_is_close(name);
    int is_military_selection_event = kbo_custom_event_name_is_military_selection(name);
    int is_asian_games_selection_event = kbo_custom_event_name_is_asian_games_selection(name);
    int is_asian_games_departure_event = kbo_custom_event_name_is_asian_games_departure(name);
    int is_asian_games_final_event = kbo_custom_event_name_is_asian_games_final(name);
    int is_cbt_deadline_event = kbo_custom_event_name_is_cbt_exception_deadline(name);
    int is_cbt_announcement_event = kbo_custom_event_name_is_cbt_announcement(name);

    if (is_open_event) {
        if (g_kbo_foreign_priority_last_open_event_fired_date == event_yyyymmdd) {
            append_logf(
                "KBO custom event skipped duplicate by date source=%s name=%s event_date=%04u-%02u-%02u",
                source != NULL ? source : "",
                name,
                event_year,
                event_month,
                event_day);
            return -1;
        }
        uint32_t serial = kbo_date_serial(event_year, event_month, event_day);
        int opened = kbo_open_foreign_waiver_window(
            event_yyyymmdd,
            serial,
            "custom_event_monitor");
        if (opened) {
            g_kbo_foreign_priority_last_open_event_fired_date = event_yyyymmdd;
        }
        return opened;
    }

    if (is_close_event) {
        if (g_kbo_foreign_priority_last_close_event_fired_date == event_yyyymmdd) {
            append_logf(
                "KBO custom event skipped duplicate by date source=%s name=%s event_date=%04u-%02u-%02u",
                source != NULL ? source : "",
                name,
                event_year,
                event_month,
                event_day);
            return -1;
        }
        append_logf(
            "KBO custom event close marker reached source=%s date=%04u-%02u-%02u",
            source != NULL ? source : "",
            event_year,
            event_month,
            event_day);
        int announced = kbo_announce_foreign_waiver_results(event_yyyymmdd, source);
        if (!announced) {
            announced = 1;
        }
        if (announced) {
            g_kbo_foreign_priority_last_close_event_fired_date = event_yyyymmdd;
        }
        return announced;
    }

    if (is_military_selection_event) {
        return run_kbo_custom_military_event(
            event_ptr,
            name,
            event_year,
            event_month,
            event_day,
            source);
    }

    if (is_asian_games_selection_event) {
        return kbo_handle_asian_games_selection_event(event_yyyymmdd, source);
    }
    if (is_asian_games_departure_event) {
        return kbo_handle_asian_games_departure_event(event_yyyymmdd, source);
    }
    if (is_asian_games_final_event) {
        return kbo_handle_asian_games_final_event(event_yyyymmdd, source);
    }
    if (is_cbt_deadline_event) {
        return kbo_handle_cbt_deadline_event(event_yyyymmdd, source);
    }
    if (is_cbt_announcement_event) {
        return kbo_handle_cbt_announcement_event(event_yyyymmdd, source);
    }

    return 0;
}
