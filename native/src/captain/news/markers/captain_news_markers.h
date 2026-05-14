#ifndef KBO_CAPTAIN_NEWS_MARKERS_H
#define KBO_CAPTAIN_NEWS_MARKERS_H

int kbo_captain_news_marker_exists(const char* key);
void kbo_captain_news_persist_marker(const char* key, const char* source);

#endif
