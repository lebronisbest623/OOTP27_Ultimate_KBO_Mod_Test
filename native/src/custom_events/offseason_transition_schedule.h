#ifndef KBOFIX_SRC_CUSTOM_EVENTS_OFFSEASON_TRANSITION_SCHEDULE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_OFFSEASON_TRANSITION_SCHEDULE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_custom_event_phase_is_offseason(uint8_t phase);
int kbo_custom_event_phase_can_enter_offseason(uint8_t phase);
int kbo_custom_event_date_allows_offseason_transition(uint32_t yyyymmdd);
int kbo_custom_event_read_league_phase(uint32_t league_id, uintptr_t* out_league_ptr, uint32_t* out_league_year, uint8_t* out_phase, uint32_t* out_phase_year);
int kbo_custom_event_schedule_pending_offseason_transition(uint32_t today_yyyymmdd, const char* source);
int kbo_custom_event_monitor_check_offseason_transition(uint32_t today_yyyymmdd, const char* source);

#endif
