#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_LEDGER_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_LEDGER_H_

#include <stddef.h>
#include <stdint.h>

#include "../names/custom_event_names.h"

int kbo_custom_event_ledger_path(char* out, size_t out_size);
int kbo_custom_event_ledger_completed(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind);
void kbo_custom_event_ledger_record(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source);

#endif
