#ifndef KBOFIX_SRC_CORE_CSV_CORE_CSV_H_
#define KBOFIX_SRC_CORE_CSV_CORE_CSV_H_

#include <stddef.h>
#include <stdint.h>

int kbo_csv_parse_field(char** cursor, char* out, size_t out_size);
int kbo_csv_parse_const_field(const char** cursor, char* out, size_t out_size);
uint32_t kbo_csv_parse_u32_text(const char* text, int base);
int kbo_csv_parse_u32_field(const char** cursor, uint32_t* out_value);
void kbo_csv_trim_token_in_place(char* text);

#endif
