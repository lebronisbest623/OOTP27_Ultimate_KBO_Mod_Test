#ifndef KBO_CORE_FLAGS_FLAG_KEY_H
#define KBO_CORE_FLAGS_FLAG_KEY_H

#include <stddef.h>

int kbo_flag_key_from_file_name(const char* file_name, char* out, size_t out_size);
const char* kbo_flag_legacy_json_key_for_key(const char* key);

#endif
