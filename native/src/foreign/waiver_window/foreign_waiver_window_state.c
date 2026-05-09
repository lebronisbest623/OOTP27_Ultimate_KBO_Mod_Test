#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/events/core_league_events.h"
#include "../../core/logging/core_log.h"
#include "../../core/dates/core_text_date.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/names/team_string.h"
#include "../common/events/foreign_priority_events.h"
#include "../common/config/foreign_waiver_config.h"
#include "../waiver_core/api/foreign_waiver_core.h"
#include "../common/dates/foreign_waiver_date.h"
#include "../common/events/foreign_waiver_events.h"
#include "../common/paths/foreign_waiver_paths.h"
#include "../common/policy/foreign_waiver_policy.h"

#include "foreign_waiver_window_internal.h"
uint32_t g_kbo_foreign_waiver_window_start_serial = 0;
uint32_t g_kbo_foreign_waiver_window_end_serial = 0;
uint32_t g_kbo_foreign_waiver_last_seen_yyyymmdd = 0;
uint32_t g_kbo_foreign_waiver_last_close_event_end = 0;
uint32_t g_kbo_foreign_waiver_start_event_date = 0;
uint32_t g_kbo_foreign_waiver_close_event_end_date = 0;
volatile LONG g_kbo_foreign_priority_pending_lock = 0;
uint32_t g_kbo_foreign_priority_pending_yyyymmdd = 0;
char g_kbo_foreign_priority_pending_title[96] = {0};
char g_kbo_foreign_priority_pending_source[48] = {0};











