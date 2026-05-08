#ifndef KBO_FOREIGN_CSV_PARSE_H
#define KBO_FOREIGN_CSV_PARSE_H

#include <stdint.h>

int parse_u32_from_csv_field(const char** cursor, uint32_t* out_value);

#endif
