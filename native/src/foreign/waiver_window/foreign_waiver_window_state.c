#include "state/foreign_waiver_window_state.h"
uint32_t g_kbo_foreign_waiver_window_start_serial = 0;
uint32_t g_kbo_foreign_waiver_window_end_serial = 0;
uint32_t g_kbo_foreign_waiver_last_seen_yyyymmdd = 0;
uint32_t g_kbo_foreign_waiver_last_close_event_end = 0;
uint32_t g_kbo_foreign_waiver_start_event_date = 0;
uint32_t g_kbo_foreign_waiver_close_event_end_date = 0;
KboLock g_kbo_foreign_priority_pending_lock = KBO_LOCK_INIT;
uint32_t g_kbo_foreign_priority_pending_yyyymmdd = 0;
char g_kbo_foreign_priority_pending_title[96] = {0};
char g_kbo_foreign_priority_pending_source[48] = {0};


