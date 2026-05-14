#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_CSV_PARSE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_CSV_PARSE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void kbo_asian_games_schedule_copy_text(char* out, size_t out_size, const char* value);
uint32_t kbo_asian_games_schedule_parse_u32(const char* text);
uint32_t kbo_asian_games_schedule_parse_date(const char* text);
uint8_t kbo_asian_games_schedule_parse_auto(const char* text);
int kbo_parse_asian_games_schedule_seed_fields(char fields[][128], int field_count, KboAsianGamesScheduleSeed* out);
int kbo_parse_asian_games_schedule_seed_line(const char* line, KboAsianGamesScheduleSeed* out);

#endif
