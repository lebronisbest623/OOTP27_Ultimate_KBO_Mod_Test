#ifndef KBO_ALLSTAR_CSV_PARSE_H
#define KBO_ALLSTAR_CSV_PARSE_H

#include <stddef.h>
#include <stdint.h>

uint8_t kbo_parse_allstar_side(const char* text);

void kbo_csv_find_allstar_team_columns(
    const char* header,
    int* year_col,
    int* team_id_col,
    int* name_col,
    int* allstar_col);

void kbo_csv_extract_allstar_team_fields(
    const char* line,
    int year_col,
    int team_id_col,
    int name_col,
    int allstar_col,
    uint16_t* year,
    char* team_id,
    size_t team_id_size,
    char* team_name,
    size_t team_name_size,
    char* current_city,
    size_t current_city_size,
    uint8_t* side);

#endif
