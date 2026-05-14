#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_NAMES_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_NAMES_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

typedef enum KboCustomEventKind {
    KBO_CUSTOM_EVENT_KIND_UNKNOWN = 0,
    KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN,
    KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE,
    KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION,
    KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION,
    KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE,
    KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL,
    KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE,
    KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT,
    KBO_CUSTOM_EVENT_KIND_COUNT
} KboCustomEventKind;

int kbo_custom_event_title_for_kind(KboCustomEventKind kind, char* out, size_t out_size);
int kbo_custom_event_name_equals_title(const char* name, const char* title);
int kbo_custom_event_name_is_kind(const char* name, KboCustomEventKind kind);

int kbo_custom_event_name_is_open(const char* name);
int kbo_custom_event_name_is_close(const char* name);
int kbo_custom_event_name_is_military_selection(const char* name);
int kbo_custom_event_name_is_asian_games_selection(const char* name);
int kbo_custom_event_name_is_asian_games_departure(const char* name);
int kbo_custom_event_name_is_asian_games_final(const char* name);
int kbo_custom_event_name_is_cbt_exception_deadline(const char* name);
int kbo_custom_event_name_is_cbt_announcement(const char* name);

#endif
