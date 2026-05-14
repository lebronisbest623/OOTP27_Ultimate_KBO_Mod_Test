#include "foreign_csv_parse.h"

#include "../../../core/csv/core_csv.h"

int parse_u32_from_csv_field(const char** cursor, uint32_t* out_value)
{
    return kbo_csv_parse_u32_field(cursor, out_value);
}
