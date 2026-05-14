#ifndef KBO_CORE_LIVE_NEWS_H
#define KBO_CORE_LIVE_NEWS_H

#include <stdint.h>

int create_kbo_native_live_news_with_body(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body);

int create_kbo_native_live_news_with_body_live_required(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body);

int create_kbo_native_live_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title);

#endif
