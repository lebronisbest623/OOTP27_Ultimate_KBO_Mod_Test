#ifndef KBOFIX_SRC_MILITARY_SERVICE_RETURNS_MILITARY_RETURN_PREVIEW_NEWS_H_
#define KBOFIX_SRC_MILITARY_SERVICE_RETURNS_MILITARY_RETURN_PREVIEW_NEWS_H_

#include <stdint.h>

int kbo_emit_military_return_preview_news_if_due(uint32_t today_serial, const char* source);

#endif
