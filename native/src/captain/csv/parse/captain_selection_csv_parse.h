#ifndef KBO_CAPTAIN_SELECTION_CSV_PARSE_H
#define KBO_CAPTAIN_SELECTION_CSV_PARSE_H

#include <stdint.h>

struct KboCaptainSelectionRow;

#define KBO_CAPTAIN_SELECTION_CSV_MAX_FIELDS 28
#define KBO_CAPTAIN_SELECTION_CSV_FIELD_SIZE 160

int kbo_captain_parse_selection_csv_fields(
    char fields[][KBO_CAPTAIN_SELECTION_CSV_FIELD_SIZE],
    int count,
    uint32_t expected_season,
    struct KboCaptainSelectionRow* out);

#endif
