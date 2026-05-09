#ifndef KBOFIX_SRC_CORE_CORE_HISTORY_STUBS_H_
#define KBOFIX_SRC_CORE_CORE_HISTORY_STUBS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void flush_pending_special_player_history_sql(const char* source);
void append_special_player_history_csv(uint32_t player_id, uint16_t season, const char* history_date, const char* event_type, const char* history_text);

#endif
