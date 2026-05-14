#include "salary_snapshot_csv_parse.h"

#include "../../core/csv/core_csv.h"

void kbo_fa_salary_snapshot_write_csv_text(HANDLE file, const char* text)
{
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        const char* p = text;
        while (*p != '\0') {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
            ++p;
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

int kbo_fa_salary_snapshot_parse_csv_field(char** cursor, char* out, size_t out_size)
{
    return kbo_csv_parse_field(cursor, out, out_size);
}

uint32_t kbo_fa_salary_snapshot_parse_u32(const char* text)
{
    return kbo_csv_parse_u32_text(text, 10);
}

int32_t kbo_fa_salary_snapshot_parse_i32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    char* tail = NULL;
    long value = strtol(text, &tail, 10);
    if (tail == text) {
        return 0;
    }
    return (int32_t)value;
}
