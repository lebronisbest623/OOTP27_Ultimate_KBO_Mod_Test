#include "independent_acquisition_request_json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void kbo_independent_acquisition_json_append_escaped(
    char* out,
    size_t out_size,
    const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (text == NULL) {
        text = "";
    }

    size_t used = strlen(out);
    for (const unsigned char* p = (const unsigned char*)text; *p != '\0' && used + 2u < out_size; p++) {
        unsigned char ch = *p;
        if (ch == '"' || ch == '\\') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = (char)ch;
        } else if (ch == '\r') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = 'r';
        } else if (ch == '\n') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = 'n';
        } else if (ch < 0x20u) {
            if (used + 7u >= out_size) { break; }
            int len = snprintf(out + used, out_size - used, "\\u%04x", (unsigned int)ch);
            if (len <= 0 || used + (size_t)len >= out_size) { break; }
            used += (size_t)len;
        } else {
            out[used++] = (char)ch;
        }
    }
    out[used] = '\0';
}

static int kbo_independent_acquisition_json_u32(
    const char* line,
    const char* key,
    uint32_t* out)
{
    if (line == NULL || key == NULL || out == NULL) {
        return 0;
    }

    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    char* end = NULL;
    unsigned long value = strtoul(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int kbo_independent_acquisition_json_i64(
    const char* line,
    const char* key,
    int64_t* out)
{
    if (line == NULL || key == NULL || out == NULL) {
        return 0;
    }

    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    char* end = NULL;
    long long value = _strtoi64(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (int64_t)value;
    return 1;
}

int kbo_independent_acquisition_parse_request_line(
    const char* line,
    KboIndependentAcquisitionQueuedRequest* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }

    KboIndependentAcquisitionQueuedRequest row;
    memset(&row, 0, sizeof(row));
    int64_t request_score = INT64_MIN;
    uint32_t value_score = 0u;
    uint32_t cash_cost = 0u;
    if (!kbo_independent_acquisition_json_u32(line, "date", &row.date)
            || !kbo_independent_acquisition_json_u32(line, "season", &row.season)
            || !kbo_independent_acquisition_json_u32(line, "buyer_team_id", &row.buyer_team_id)
            || !kbo_independent_acquisition_json_u32(line, "seller_team_id", &row.seller_team_id)
            || !kbo_independent_acquisition_json_u32(line, "player_id", &row.player_id)
            || !kbo_independent_acquisition_json_i64(line, "request_score", &request_score)) {
        return 0;
    }
    kbo_independent_acquisition_json_u32(line, "value_score", &value_score);
    kbo_independent_acquisition_json_u32(line, "cash_cost", &cash_cost);
    row.request_score = request_score;
    row.value_score = (int32_t)value_score;
    row.cash_cost = (int32_t)cash_cost;
    *out = row;
    return row.season != 0u
        && row.buyer_team_id != 0u
        && row.seller_team_id != 0u
        && row.player_id != 0u;
}
