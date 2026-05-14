#include "core_policy.h"

#include "../core_flags/localappdata/localappdata_reader.h"

int32_t kbo_read_clamped_policy_int(
    const char* file_name,
    const char* key,
    int32_t fallback,
    int32_t min_value,
    int32_t max_value)
{
    int value = (int)fallback;
    if (kbo_read_localappdata_named_json_int_value(file_name, key, &value)
            && value >= min_value
            && value <= max_value) {
        return (int32_t)value;
    }
    return fallback;
}
