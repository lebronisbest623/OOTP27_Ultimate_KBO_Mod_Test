#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_NAMES_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_NAMES_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_custom_event_name_is_open(const char* name);
int kbo_custom_event_name_is_close(const char* name);
int kbo_custom_event_name_is_military_selection(const char* name);
int kbo_custom_event_name_is_asian_games_selection(const char* name);
int kbo_custom_event_name_is_asian_games_departure(const char* name);
int kbo_custom_event_name_is_asian_games_final(const char* name);
int kbo_custom_event_name_is_cbt_exception_deadline(const char* name);
int kbo_custom_event_name_is_cbt_announcement(const char* name);

#endif
