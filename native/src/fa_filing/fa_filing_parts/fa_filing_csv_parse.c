#include "fa_filing_csv_parse.h"

#include "../../core/csv/core_csv.h"

uint32_t kbo_fa_filing_parse_u32(const char* text)
{
    return kbo_csv_parse_u32_text(text, 0);
}

void kbo_fa_filing_copy_text(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (text == NULL) {
        return;
    }
    size_t used = 0;
    while (text[used] != '\0' && used + 1u < out_size) {
        out[used] = text[used];
        used++;
    }
    out[used] = '\0';
}

int kbo_fa_filing_parse_csv_field(char** cursor, char* out, size_t out_size)
{
    return kbo_csv_parse_field(cursor, out, out_size);
}
