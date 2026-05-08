#include "foreign_replacement_seed_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int kbo_ascii_is_seed_id_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

void kbo_trim_csv_token_in_place(char* text)
{
    if (text == NULL) {
        return;
    }
    char* start = text;
    while (*start == ' ' || *start == '\t' || *start == '"') {
        start++;
    }
    char* end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n' || end[-1] == '"')) {
        end--;
    }
    *end = '\0';
    if (start != text) {
        memmove(text, start, strlen(start) + 1u);
    }
}

uint8_t kbo_parse_foreign_replacement_seed_slot_type(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    if (text[0] == '2') {
        return KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA;
    }
    if (text[0] == '1') {
        return KBO_FOREIGN_INJURY_SLOT_REGULAR;
    }
    if (_stricmp(text, "asian") == 0 || _stricmp(text, "asian_quota") == 0 || _stricmp(text, "asia") == 0) {
        return KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA;
    }
    if (_stricmp(text, "regular") == 0 || _stricmp(text, "foreign") == 0) {
        return KBO_FOREIGN_INJURY_SLOT_REGULAR;
    }
    return 0u;
}

int kbo_parse_foreign_replacement_player_seed_line(
    const char* line,
    KboForeignReplacementPlayerSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char copy[160] = {0};
    size_t len = strlen(line);
    if (len >= sizeof(copy)) {
        len = sizeof(copy) - 1u;
    }
    memcpy(copy, line, len);

    char* p = copy;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';') {
        return 0;
    }

    char* first = p;
    char* comma = strchr(first, ',');
    char* second = NULL;
    if (comma != NULL) {
        *comma = '\0';
        second = comma + 1;
        char* second_comma = strchr(second, ',');
        if (second_comma != NULL) {
            *second_comma = '\0';
        }
    }

    kbo_trim_csv_token_in_place(first);
    if (second != NULL) {
        kbo_trim_csv_token_in_place(second);
    }
    if (first[0] == '\0' || _stricmp(first, "replacement_player_key") == 0 || _stricmp(first, "player_id") == 0) {
        return 0;
    }

    size_t key_len = strlen(first);
    if (key_len >= sizeof(out->key)) {
        key_len = sizeof(out->key) - 1u;
    }
    memcpy(out->key, first, key_len);
    out->key[key_len] = '\0';
    out->slot_type = kbo_parse_foreign_replacement_seed_slot_type(second);
    if (first[0] >= '0' && first[0] <= '9') {
        out->player_id = (uint32_t)strtoul(first, NULL, 10);
    }
    return out->key[0] != '\0' || out->player_id != 0u;
}
