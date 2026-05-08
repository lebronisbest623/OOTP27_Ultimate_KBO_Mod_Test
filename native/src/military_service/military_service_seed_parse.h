#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_SEED_PARSE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_SEED_PARSE_H_

#include <stdint.h>

#ifndef KBO_MILITARY_SERVICE_DAYS
#define KBO_MILITARY_SERVICE_DAYS 545
#endif

typedef struct KboMilitaryServiceSeed {
    char key[40];
    uint32_t player_id;
    char service_team_code[12];
    char original_team_code[12];
    uint32_t service_start_yyyymmdd;
    uint32_t service_return_yyyymmdd;
    int32_t service_total_days;
} KboMilitaryServiceSeed;

void kbo_military_trim_csv_token_in_place(char* text);
int kbo_military_ascii_is_seed_id_char(char ch);
int kbo_military_parse_u32_full_token(const char* text, uint32_t* out);
uint32_t kbo_military_parse_yyyymmdd(const char* text);
uint32_t kbo_military_yyyymmdd_to_serial(uint32_t yyyymmdd);
uint32_t kbo_military_days_in_month(uint32_t year, uint32_t month);
uint32_t kbo_military_serial_to_yyyymmdd(uint32_t serial);
uint32_t kbo_military_yyyymmdd_add_days(uint32_t yyyymmdd, int32_t days);
int32_t kbo_military_days_left_from_return_serial(uint32_t return_serial, uint32_t today_serial);
int kbo_parse_military_service_seed_line(const char* line, KboMilitaryServiceSeed* out);

#endif
