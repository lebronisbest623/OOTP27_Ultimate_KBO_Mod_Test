#ifndef KBOFIX_SRC_FA_FILING_FA_FILING_PARTS_FA_FILING_CSV_PARSE_H_
#define KBOFIX_SRC_FA_FILING_FA_FILING_PARTS_FA_FILING_CSV_PARSE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint32_t kbo_fa_filing_parse_u32(const char* text);
void kbo_fa_filing_copy_text(char* out, size_t out_size, const char* text);
int kbo_fa_filing_parse_csv_field(char** cursor, char* out, size_t out_size);

#endif
