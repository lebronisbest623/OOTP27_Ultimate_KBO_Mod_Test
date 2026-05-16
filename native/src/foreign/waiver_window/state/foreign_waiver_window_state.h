#ifndef KBOFIX_SRC_FOREIGN_WAIVER_WINDOW_STATE_H_
#define KBOFIX_SRC_FOREIGN_WAIVER_WINDOW_STATE_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../core/sync/lock.h"

extern uint32_t g_kbo_foreign_waiver_window_start_serial;
extern uint32_t g_kbo_foreign_waiver_window_end_serial;
extern uint32_t g_kbo_foreign_waiver_last_seen_yyyymmdd;
extern uint32_t g_kbo_foreign_waiver_last_close_event_end;
extern uint32_t g_kbo_foreign_waiver_start_event_date;
extern uint32_t g_kbo_foreign_waiver_close_event_end_date;
extern KboLock g_kbo_foreign_priority_pending_lock;
extern uint32_t g_kbo_foreign_priority_pending_yyyymmdd;
extern char g_kbo_foreign_priority_pending_title[96];
extern char g_kbo_foreign_priority_pending_source[48];

#endif
