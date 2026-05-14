#ifndef KBO_CORE_FLAGS_JSON_BOOL_PARSER_H
#define KBO_CORE_FLAGS_JSON_BOOL_PARSER_H

#include <stddef.h>
#include <windows.h>

#define KBO_FLAGS_JSON_MAX_BYTES (64u * 1024u)

const char* kbo_json_skip_ws(const char* p, const char* end);
const char* kbo_json_find_string_end(const char* quote, const char* end);
int kbo_json_string_equals_key(const char* start, const char* stop, const char* key);
int kbo_json_bool_value_at(const char* value, const char* end, int* out_value);
int kbo_json_int_value_at(const char* value, const char* end, int* out_value);
int kbo_json_string_value_at(const char* value, const char* end, char* out, size_t out_size);
int kbo_find_flag_value_in_json(const char* json, DWORD json_size, const char* key, int* out_value);
int kbo_find_int_value_in_json(const char* json, DWORD json_size, const char* key, int* out_value);
int kbo_find_string_value_in_json(const char* json, DWORD json_size, const char* key, char* out, size_t out_size);
int kbo_find_json_value_span(const char* json, DWORD json_size, const char* key, const char** out_start, const char** out_end);

#endif
