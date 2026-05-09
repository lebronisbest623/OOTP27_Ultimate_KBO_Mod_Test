#ifndef KBOFIX_SRC_CORE_CORE_NEWS_OBJECT_H_
#define KBOFIX_SRC_CORE_CORE_NEWS_OBJECT_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int assign_kbo_news_pointer_string(void* news_object, uint32_t offset, const char* text);
uint32_t kbo_read_news_u32(void* object, uint32_t offset);
int create_kbo_real_add_news(uint32_t year, uint32_t month, uint32_t day, uint32_t league_id, uint32_t message_type, const char* title, const char* body, const char* source);

#endif
