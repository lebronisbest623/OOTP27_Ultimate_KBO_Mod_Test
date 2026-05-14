#include "core_sql_escape.h"

static int kbo_sql_should_keep_control(unsigned char ch, int preserve_ootp_controls)
{
    if (ch >= 0x20u) {
        return 1;
    }
    if (ch == '\t') {
        return 1;
    }
    return preserve_ootp_controls && (ch == '\n' || ch == '\r' || ch == 0x01u);
}

static int kbo_sql_escape_literal_core(char* out, size_t out_size, const char* in, int preserve_ootp_controls)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }

    size_t write = 0;
    if (in != NULL) {
        for (const char* p = in; *p != '\0'; p++) {
            unsigned char ch = (unsigned char)*p;
            if (!kbo_sql_should_keep_control(ch, preserve_ootp_controls)) {
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

int kbo_sql_escape_literal(char* out, size_t out_size, const char* in)
{
    return kbo_sql_escape_literal_core(out, out_size, in, 0);
}

int kbo_sql_escape_literal_preserve_ootp_controls(char* out, size_t out_size, const char* in)
{
    return kbo_sql_escape_literal_core(out, out_size, in, 1);
}
