#ifndef KBOFIX_SRC_CORE_NEWS_LEDGER_H_
#define KBOFIX_SRC_CORE_NEWS_LEDGER_H_

#include <stddef.h>

int kbo_custom_news_ledger_path(char* out, size_t out_size);
int kbo_custom_news_ledger_completed(const char* domain, const char* key);
void kbo_custom_news_ledger_record(
    const char* domain,
    const char* key,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source);
void kbo_custom_news_ledger_record_completed(
    const char* domain,
    const char* key,
    const char* detail,
    const char* source);

#endif
