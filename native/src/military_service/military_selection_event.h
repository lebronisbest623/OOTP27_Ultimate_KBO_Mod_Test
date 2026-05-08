#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SELECTION_EVENT_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SELECTION_EVENT_H_

#include <stdint.h>
#include <stddef.h>

int run_kbo_custom_military_event(
    uintptr_t event_ptr,
    const char* event_name,
    uint32_t event_year,
    uint32_t event_month,
    uint32_t event_day,
    const char* source);
int kbo_refresh_military_selection_candidates_from_memory(
    uint16_t entry_year,
    uint32_t sang_id,
    const char* source);
void kbo_military_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size);

#endif
