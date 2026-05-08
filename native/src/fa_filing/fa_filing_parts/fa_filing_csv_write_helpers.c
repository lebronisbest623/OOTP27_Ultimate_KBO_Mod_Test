#include "fa_filing_csv_write_helpers.h"

void kbo_fa_filing_write_raw(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

void kbo_fa_filing_write_csv_text(HANDLE file, const char* text)
{
    kbo_fa_filing_write_raw(file, "\"");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                kbo_fa_filing_write_raw(file, "\"\"");
            } else {
                char one[2] = {*p, '\0'};
                kbo_fa_filing_write_raw(file, one);
            }
        }
    }
    kbo_fa_filing_write_raw(file, "\"");
}
