#include "../internal/foreign_injury_internal.h"

static void kbo_foreign_injury_trim_token(char* text)
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

static int kbo_foreign_injury_token_is_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    for (const char* p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
    }
    return 1;
}

static uint32_t kbo_foreign_injury_parse_player_key_token(const char* text, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    return kbo_resolve_foreign_replacement_player_seed_key(text, out_slot_type);
}

static uint8_t kbo_foreign_injury_parse_status_token(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    if (kbo_foreign_injury_token_is_u32(text)) {
        return (uint8_t)strtoul(text, NULL, 10);
    }
    if (_stricmp(text, "open") == 0) {
        return KBO_FOREIGN_INJURY_STATUS_OPEN;
    }
    if (_stricmp(text, "active") == 0) {
        return KBO_FOREIGN_INJURY_STATUS_ACTIVE;
    }
    if (_stricmp(text, "pending") == 0 || _stricmp(text, "decision") == 0) {
        return KBO_FOREIGN_INJURY_STATUS_PENDING;
    }
    if (_stricmp(text, "closed") == 0) {
        return KBO_FOREIGN_INJURY_STATUS_CLOSED;
    }
    return 0u;
}

int kbo_parse_foreign_injury_replacement_key_seed_line(
    const char* line,
    uint32_t today,
    KboForeignInjuryReplacement* out)
{
    char copy[256] = {0};
    size_t len = strlen(line);
    if (len >= sizeof(copy)) {
        len = sizeof(copy) - 1u;
    }
    memcpy(copy, line, len);

    char* tokens[9] = {0};
    int count = 0;
    char* cursor = copy;
    while (count < 9 && cursor != NULL) {
        tokens[count++] = cursor;
        char* comma = strchr(cursor, ',');
        if (comma == NULL) {
            break;
        }
        *comma = '\0';
        cursor = comma + 1;
    }
    for (int i = 0; i < count; i++) {
        kbo_foreign_injury_trim_token(tokens[i]);
    }

    if (count < 3
            || tokens[0][0] == '\0'
            || _stricmp(tokens[0], "team_id") == 0
            || (_stricmp(tokens[1], "injured_player_id") == 0)) {
        return 0;
    }

    int has_key_token = 0;
    for (int i = 1; i < count && i <= 2; i++) {
        if (!kbo_foreign_injury_token_is_u32(tokens[i])) {
            has_key_token = 1;
        }
    }
    if (!has_key_token && (count < 4 || kbo_foreign_injury_token_is_u32(tokens[3]))) {
        return 0;
    }

    out->team_id = kbo_foreign_injury_token_is_u32(tokens[0]) ? (uint32_t)strtoul(tokens[0], NULL, 10) : 0u;
    uint8_t injured_slot_type = 0u;
    uint8_t replacement_slot_type = 0u;
    out->injured_player_id = kbo_foreign_injury_parse_player_key_token(tokens[1], &injured_slot_type);
    out->replacement_player_id = kbo_foreign_injury_parse_player_key_token(tokens[2], &replacement_slot_type);
    out->opened_on_yyyymmdd = today;
    out->slot_type = count >= 4 ? kbo_parse_foreign_replacement_seed_slot_type(tokens[3]) : 0u;
    out->status = count >= 5 ? kbo_foreign_injury_parse_status_token(tokens[4]) : 0u;

    if (out->slot_type == 0u) {
        out->slot_type = injured_slot_type != 0u ? injured_slot_type : replacement_slot_type;
    }
    if (out->status == 0u) {
        out->status = out->replacement_player_id != 0u ? KBO_FOREIGN_INJURY_STATUS_ACTIVE : KBO_FOREIGN_INJURY_STATUS_OPEN;
    }
    return out->injured_player_id != 0u;
}
