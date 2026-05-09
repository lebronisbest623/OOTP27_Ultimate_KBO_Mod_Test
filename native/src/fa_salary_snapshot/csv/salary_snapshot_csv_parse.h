#ifndef KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_CSV_PARSE_H_
#define KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_CSV_PARSE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void kbo_fa_salary_snapshot_write_csv_text(HANDLE file, const char* text);
int kbo_fa_salary_snapshot_parse_csv_field(char** cursor, char* out, size_t out_size);
uint32_t kbo_fa_salary_snapshot_parse_u32(const char* text);
int32_t kbo_fa_salary_snapshot_parse_i32(const char* text);

#endif
