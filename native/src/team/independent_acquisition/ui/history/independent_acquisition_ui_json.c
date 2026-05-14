#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ui_history_internal.h"
#include "../../ai/independent_acquisition_ai_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../lookup/team_lookup.h"

static void kbo_independent_acquisition_ui_copy_text(
    char* out,
    size_t out_size,
    const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (text == NULL) {
        return;
    }
    snprintf(out, out_size, "%s", text);
}
static int kbo_independent_acquisition_ui_data_path(
    const char* filename,
    char* out,
    size_t out_size)
{
    if (filename == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_save_scoped_data_file(filename, out, out_size);
}

char* kbo_independent_acquisition_ui_read_text_file(const char* filename, DWORD* out_read)
{
    if (out_read != NULL) {
        *out_read = 0u;
    }
    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_ui_data_path(filename, path, sizeof(path))) {
        return NULL;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 8u * 1024u * 1024u) {
        CloseHandle(file);
        return NULL;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return NULL;
    }

    DWORD read = 0u;
    if (!ReadFile(file, buffer, size, &read, NULL) || read == 0u) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return NULL;
    }
    buffer[read] = '\0';
    CloseHandle(file);
    if (out_read != NULL) {
        *out_read = read;
    }
    return buffer;
}

static int kbo_independent_acquisition_ui_json_u32(
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

static int kbo_independent_acquisition_ui_json_i32(
    const char* line,
    const char* key,
    int32_t* out)
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
    long value = strtol(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (int32_t)value;
    return 1;
}

static int kbo_independent_acquisition_ui_json_i64(
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

static int kbo_independent_acquisition_ui_json_string(
    const char* line,
    const char* key,
    char* out,
    size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (line == NULL || key == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":\"", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    size_t used = 0u;
    while (*p != '\0' && *p != '"' && used + 1u < out_size) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
        }
        out[used++] = *p++;
    }
    out[used] = '\0';
    return used > 0u;
}

int kbo_independent_acquisition_ui_parse_request_line(
    const char* line,
    KboIndependentAcquisitionUiRequestRow* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    KboIndependentAcquisitionUiRequestRow row;
    memset(&row, 0, sizeof(row));
    if (!kbo_independent_acquisition_ui_json_u32(line, "date", &row.date)
            || !kbo_independent_acquisition_ui_json_u32(line, "season", &row.season)
            || !kbo_independent_acquisition_ui_json_u32(line, "buyer_team_id", &row.buyer_team_id)
            || !kbo_independent_acquisition_ui_json_u32(line, "seller_team_id", &row.seller_team_id)
            || !kbo_independent_acquisition_ui_json_u32(line, "player_id", &row.player_id)
            || !kbo_independent_acquisition_ui_json_i64(line, "request_score", &row.request_score)) {
        return 0;
    }

    uint32_t tmp = 0u;
    kbo_independent_acquisition_ui_json_u32(line, "nation_id", &row.nation_id);
    if (kbo_independent_acquisition_ui_json_u32(line, "pitcher", &tmp)) {
        row.pitcher = tmp ? 1u : 0u;
    }
    tmp = 0u;
    if (kbo_independent_acquisition_ui_json_u32(line, "asian_quota", &tmp)) {
        row.asian_quota = tmp ? 1u : 0u;
    }
    kbo_independent_acquisition_ui_json_i32(line, "cash_cost", &row.cash_cost);
    kbo_independent_acquisition_ui_json_i32(line, "value_score", &row.value_score);
    kbo_independent_acquisition_ui_json_u32(line, "effective_before", &row.effective_before);
    kbo_independent_acquisition_ui_json_u32(line, "effective_after", &row.effective_after);
    kbo_independent_acquisition_ui_json_u32(line, "effective_limit", &row.effective_limit);
    kbo_independent_acquisition_ui_json_u32(line, "injured_player_id", &row.injured_player_id);
    kbo_independent_acquisition_ui_json_string(line, "slot_type", row.slot_label, sizeof(row.slot_label));
    if (row.slot_label[0] == '\0') {
        kbo_independent_acquisition_ui_copy_text(row.slot_label, sizeof(row.slot_label), "-");
    }

    uint32_t current_team_id = 0u;
    uint32_t current_league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(row.player_id, &current_team_id, &current_league_id);
    (void)current_team_id;
    (void)current_league_id;
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        row.player_ptr = (uintptr_t)player;
        if (memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))) {
            row.age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        }
        if (row.nation_id == 0u) {
            row.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        }
    }

    *out = row;
    return row.season != 0u
        && row.buyer_team_id != 0u
        && row.seller_team_id != 0u
        && row.player_id != 0u;
}

int kbo_independent_acquisition_ui_parse_decision_line(
    const char* line,
    KboIndependentAcquisitionUiDecisionRow* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    KboIndependentAcquisitionUiDecisionRow row;
    memset(&row, 0, sizeof(row));
    if (!kbo_independent_acquisition_ui_json_u32(line, "date", &row.date)
            || !kbo_independent_acquisition_ui_json_u32(line, "season", &row.season)
            || !kbo_independent_acquisition_ui_json_u32(line, "seller_team_id", &row.seller_team_id)
            || !kbo_independent_acquisition_ui_json_u32(line, "player_id", &row.player_id)
            || !kbo_independent_acquisition_ui_json_u32(line, "buyer_team_id", &row.buyer_team_id)
            || !kbo_independent_acquisition_ui_json_i64(line, "request_score", &row.request_score)) {
        return 0;
    }
    kbo_independent_acquisition_ui_json_i32(line, "value_score", &row.value_score);
    kbo_independent_acquisition_ui_json_i32(line, "cash_cost", &row.cash_cost);
    kbo_independent_acquisition_ui_json_i32(line, "old_cash", &row.old_cash);
    kbo_independent_acquisition_ui_json_i32(line, "new_cash", &row.new_cash);
    uint32_t transferred = 0u;
    if (kbo_independent_acquisition_ui_json_u32(line, "transferred", &transferred)) {
        row.transferred = transferred ? 1u : 0u;
    }
    *out = row;
    return row.season != 0u && row.seller_team_id != 0u && row.player_id != 0u;
}
