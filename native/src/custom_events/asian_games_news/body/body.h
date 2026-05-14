#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_BODY_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_NEWS_BODY_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_asian_games_append_player_blurb(char* out, size_t out_size, size_t* used, KboAsianGamesRosterEntry* entry, LONG display_index, LONG display_count);
int kbo_asian_games_append_roster_line(char* out, size_t out_size, size_t* used, LONG index, KboAsianGamesRosterEntry* entry);
KboAsianGamesRosterEntry* kbo_asian_games_choose_captain(void);
void kbo_asian_games_build_news_context(
    uint32_t event_yyyymmdd,
    char* roster_year_text,
    size_t roster_year_size,
    char* host_city,
    size_t host_city_size,
    char* host_country,
    size_t host_country_size,
    char* host_place,
    size_t host_place_size,
    char* tournament_label,
    size_t tournament_label_size,
    char* tournament_phrase,
    size_t tournament_phrase_size);
void kbo_asian_games_build_final_matchup(
    uint32_t event_yyyymmdd,
    int gold_won,
    char* final_opponent,
    size_t final_opponent_size,
    char* final_score,
    size_t final_score_size,
    char* korea_score,
    size_t korea_score_size,
    char* opponent_score,
    size_t opponent_score_size);
void kbo_build_asian_games_news_body(char* out, size_t out_size, uint32_t event_yyyymmdd, const char* template_prefix, const char* lead, const char* source);

#endif
