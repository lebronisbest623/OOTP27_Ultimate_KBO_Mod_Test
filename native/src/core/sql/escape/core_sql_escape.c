#include "core_sql_escape.h"

int kbo_sql_escape_literal(char* out, size_t out_size, const char* in)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }

    size_t write = 0;
    if (in != NULL) {
        for (const char* p = in; *p != '\0'; p++) {
            if ((unsigned char)*p < 0x20 && *p != '\t') {
                continue;
            }

            size_t needed = (*p == '\'') ? 2u : 1u;
            if (write + needed >= out_size) {
                out[0] = '\0';
                return 0;
            }

            if (*p == '\'') {
                out[write++] = '\'';
                out[write++] = '\'';
            } else {
                out[write++] = *p;
            }
        }
    }

    out[write] = '\0';
    return 1;
}
