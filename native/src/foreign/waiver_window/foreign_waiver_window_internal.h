#ifndef NATIVE_SRC_FOREIGN_FOREIGN_WAIVER_WINDOW_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_FOREIGN_WAIVER_WINDOW_C_INTERNAL_H

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
#if defined(__GNUC__)
#endif

extern uint32_t g_kbo_foreign_waiver_window_start_serial;
extern uint32_t g_kbo_foreign_waiver_window_end_serial;
extern uint32_t g_kbo_foreign_waiver_last_seen_yyyymmdd;
extern uint32_t g_kbo_foreign_waiver_last_close_event_end;
extern uint32_t g_kbo_foreign_waiver_start_event_date;
extern uint32_t g_kbo_foreign_waiver_close_event_end_date;
extern volatile LONG g_kbo_foreign_priority_pending_lock;
extern uint32_t g_kbo_foreign_priority_pending_yyyymmdd;
extern char g_kbo_foreign_priority_pending_title[96];
extern char g_kbo_foreign_priority_pending_source[48];

void kbo_queue_foreign_priority_league_event(
    uint32_t event_yyyymmdd,
    const char* title,
    const char* source);
void kbo_flush_pending_foreign_priority_events(const char* source);
uint32_t kbo_detect_offseason_starts_event(uint32_t today_yyyymmdd, uint32_t league_id);
int kbo_write_foreign_waiver_window(uint32_t start_yyyymmdd, uint32_t end_yyyymmdd, const char* reason);
int kbo_open_foreign_waiver_window(uint32_t today_yyyymmdd, uint32_t today_serial, const char* reason);
int kbo_advance_foreign_waiver_window(uint32_t today_yyyymmdd, uint32_t today_serial);
int kbo_read_foreign_waiver_window(uint32_t* out_start, uint32_t* out_end);
int kbo_read_foreign_waiver_window_cached(uint32_t today, uint32_t* out_start, uint32_t* out_end);
int kbo_is_foreign_waiver_negotiation_window_open(void);
int kbo_format_ymd(uint32_t yyyymmdd, char* out, size_t out_size);
int kbo_get_foreign_waiver_window_status_text(char* out, size_t out_size);
int kbo_current_foreign_waiver_window_dates(uint32_t* out_start, uint32_t* out_end);

#endif
