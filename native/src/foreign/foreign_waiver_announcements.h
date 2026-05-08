#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_ANNOUNCEMENTS_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_ANNOUNCEMENTS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_foreign_waiver_announcement_recorded(uint32_t event_yyyymmdd);
void kbo_record_foreign_waiver_announcement(uint32_t event_yyyymmdd);
void kbo_record_foreign_waiver_announcement_body(uint32_t event_yyyymmdd, const char* source, const char* body);

#endif
