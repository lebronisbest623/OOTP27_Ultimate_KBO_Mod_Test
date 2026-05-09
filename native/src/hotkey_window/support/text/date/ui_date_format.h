#ifndef KBO_HOTKEY_WINDOW_UI_DATE_FORMAT_H
#define KBO_HOTKEY_WINDOW_UI_DATE_FORMAT_H

#include <stddef.h>
#include <stdint.h>

int kbo_hub_days_until_yyyymmdd(uint32_t yyyymmdd);
const char* kbo_hub_weekday_abbrev(int weekday);
void kbo_hub_format_ootp_date(uint32_t year, uint32_t month, uint32_t day, char* out, size_t out_size);

#endif
