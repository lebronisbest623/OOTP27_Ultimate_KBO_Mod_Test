#ifndef KBOFIX_SRC_FA_FILING_FA_FILING_PARTS_FA_FILING_CSV_WRITE_HELPERS_H_
#define KBOFIX_SRC_FA_FILING_FA_FILING_PARTS_FA_FILING_CSV_WRITE_HELPERS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void kbo_fa_filing_write_raw(HANDLE file, const char* text);
void kbo_fa_filing_write_csv_text(HANDLE file, const char* text);

#endif
