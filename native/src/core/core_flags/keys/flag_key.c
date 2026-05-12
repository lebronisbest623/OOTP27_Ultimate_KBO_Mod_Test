#include "flag_key.h"

#include <string.h>

static char kbo_flag_key_ascii_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static int kbo_flag_key_suffix_equals_ignore_case(const char* text, const char* suffix)
{
    if (text == NULL || suffix == NULL) {
        return 0;
    }

    while (*text != '\0' && *suffix != '\0') {
        if (kbo_flag_key_ascii_lower(*text) != kbo_flag_key_ascii_lower(*suffix)) {
            return 0;
        }
        text++;
        suffix++;
    }

    return *text == '\0' && *suffix == '\0';
}

int kbo_flag_key_from_file_name(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }

    const char* base = file_name;
    for (const char* p = file_name; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/') {
            base = p + 1;
        }
    }

    size_t len = strlen(base);
    if (len > 4 && kbo_flag_key_suffix_equals_ignore_case(base + len - 4, ".txt")) {
        len -= 4;
    }
    if (len == 0 || len >= out_size) {
        return 0;
    }

    memcpy(out, base, len);
    out[len] = '\0';
    return 1;
}

const char* kbo_flag_legacy_json_key_for_key(const char* key)
{
    if (key == NULL) {
        return NULL;
    }

    if (strcmp(key, "disable_kbo_no_minor_contract_patch") == 0) {
        return "disable_kbo_no_minor_contract_experimental_patch";
    }

    if (strcmp(key, "enable_foreign_ai_roster_management") == 0) {
        return "enable_foreign_ai_roster_research_hooks";
    }

    return NULL;
}
