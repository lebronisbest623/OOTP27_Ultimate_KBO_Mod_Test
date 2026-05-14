#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_MARKERS_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_MARKERS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include "../names/custom_event_names.h"

int kbo_get_custom_event_processed_marker_path(char* out, size_t out_size);
int kbo_custom_event_processed_marker_exists(uint32_t event_yyyymmdd, const char* name);
int kbo_custom_event_processed_marker_exists_for_kind(uint32_t event_yyyymmdd, KboCustomEventKind kind);
void kbo_persist_custom_event_processed_marker(uint32_t event_yyyymmdd, const char* name, const char* source);
void kbo_mark_custom_event_over(uintptr_t event_ptr);
int kbo_custom_event_already_processed(uintptr_t event_ptr);
void kbo_mark_custom_event_processed(uintptr_t event_ptr);

#endif
