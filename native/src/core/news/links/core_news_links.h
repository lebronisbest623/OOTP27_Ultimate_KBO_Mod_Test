#ifndef KBOFIX_SRC_CORE_NEWS_LINKS_CORE_NEWS_LINKS_H_
#define KBOFIX_SRC_CORE_NEWS_LINKS_CORE_NEWS_LINKS_H_

#include <stdint.h>

#define KBO_NEWS_RELATED_PLAYER_MAX 5
#define KBO_NEWS_RELATED_TEAM_MAX 5

typedef struct KboNewsRelatedIds {
    uint32_t player_ids[KBO_NEWS_RELATED_PLAYER_MAX];
    uint32_t team_ids[KBO_NEWS_RELATED_TEAM_MAX];
    int player_count;
    int team_count;
} KboNewsRelatedIds;

void kbo_news_related_ids_init(KboNewsRelatedIds* out);
void kbo_news_related_ids_collect(KboNewsRelatedIds* out, const char* text);
void kbo_news_related_ids_collect_pair(KboNewsRelatedIds* out, const char* title, const char* body);

#endif
