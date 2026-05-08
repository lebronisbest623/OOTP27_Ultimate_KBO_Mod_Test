#ifndef KBO_CORE_SQL_ESCAPE_H
#define KBO_CORE_SQL_ESCAPE_H

#include <stddef.h>

int kbo_sql_escape_literal(char* out, size_t out_size, const char* in);

#endif
