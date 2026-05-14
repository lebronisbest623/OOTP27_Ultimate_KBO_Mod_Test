#ifndef KBOFIX_SRC_CUSTOM_EVENTS_RUNTIME_MARKERS_CUSTOM_EVENT_MARKER_PRUNE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_RUNTIME_MARKERS_CUSTOM_EVENT_MARKER_PRUNE_H_

#include <stdint.h>
#include <stddef.h>

uint32_t kbo_custom_event_marker_parse_date(const char* line, size_t line_len);
void kbo_prune_rewound_custom_event_markers(const char* source);

#endif
