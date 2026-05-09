#include "../common/custom_events_common.h"
#include "custom_event_names.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

int kbo_custom_event_name_is_open(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_foreign_priority_open_event_title);
}

int kbo_custom_event_name_is_close(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_foreign_priority_close_event_title);
}

int kbo_custom_event_name_is_military_selection(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_military_selection_event_title);
}

int kbo_custom_event_name_is_asian_games_selection(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_selection_event_title);
}

int kbo_custom_event_name_is_asian_games_departure(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_departure_event_title);
}

int kbo_custom_event_name_is_asian_games_final(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_final_event_title);
}
