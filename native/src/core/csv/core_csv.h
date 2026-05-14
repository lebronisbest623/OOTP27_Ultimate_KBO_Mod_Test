#ifndef KBOFIX_SRC_CORE_CSV_CORE_CSV_H_
#define KBOFIX_SRC_CORE_CSV_CORE_CSV_H_

#include <stddef.h>
#include <stdint.h>

typedef struct KboCsvReader KboCsvReader;

int kbo_csv_parse_field(char** cursor, char* out, size_t out_size);
int kbo_csv_parse_const_field(const char** cursor, char* out, size_t out_size);
uint32_t kbo_csv_parse_u32_text(const char* text, int base);
int kbo_csv_parse_u32_field(const char** cursor, uint32_t* out_value);
void kbo_csv_trim_token_in_place(char* text);
int kbo_csv_read_line_fields(const char* line, char* fields, size_t field_size, int max_fields);
int kbo_csv_read_trimmed_line_fields(const char* line, char* fields, size_t field_size, int max_fields);
int kbo_csv_copy_line_field(const char* line, int target_index, char* out, size_t out_size);

KboCsvReader* kbo_csv_reader_open(const char* path);
void kbo_csv_reader_close(KboCsvReader* reader);
int kbo_csv_reader_next_row(KboCsvReader* reader);
int kbo_csv_reader_next_field(KboCsvReader* reader, char* out, size_t out_size);
int kbo_csv_reader_read_fields(KboCsvReader* reader, char* fields, size_t field_size, int max_fields);
int kbo_csv_reader_read_trimmed_fields(KboCsvReader* reader, char* fields, size_t field_size, int max_fields);

#endif
