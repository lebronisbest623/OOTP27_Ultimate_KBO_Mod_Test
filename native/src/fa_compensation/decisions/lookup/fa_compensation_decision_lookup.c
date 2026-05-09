#include "../fa_compensation_decisions_internal.h"

int kbo_load_latest_fa_compensation_decision(
    uint32_t fa_player_id,
    KboFaCompensationDecisionRow* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (fa_player_id == 0u || out == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_decisions_path(path, sizeof(path))) {
        return 0;
    }
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }
    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }
    DWORD read = 0;
    if (!ReadFile(file, buffer, size, &read, NULL)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    buffer[read] = '\0';

    int found = 0;
    char* cursor = buffer;
    while (*cursor != '\0') {
        char* next = strchr(cursor, '\n');
        if (next != NULL) {
            *next = '\0';
        }
        char* p = cursor;
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            char fields[16][128];
            memset(fields, 0, sizeof(fields));
            int ok = 1;
            for (int field_index = 0; field_index < 16; field_index++) {
                if (!kbo_fa_compensation_parse_csv_field(&p, fields[field_index], sizeof(fields[field_index]))) {
                    ok = 0;
                    break;
                }
            }
            if (ok && kbo_fa_compensation_parse_u32(fields[0]) == fa_player_id) {
                out->fa_player_id = kbo_fa_compensation_parse_u32(fields[0]);
                out->season = kbo_fa_compensation_parse_u32(fields[1]);
                kbo_fa_compensation_copy_token(fields[2], out->grade, sizeof(out->grade));
                out->original_team_id = kbo_fa_compensation_parse_u32(fields[3]);
                out->signing_team_id = kbo_fa_compensation_parse_u32(fields[4]);
                out->signed_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[5]);
                out->due_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[6]);
                out->decided_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[7]);
                kbo_fa_compensation_copy_token(fields[8], out->action, sizeof(out->action));
                out->selected_player_id = kbo_fa_compensation_parse_u32(fields[9]);
                kbo_fa_compensation_copy_token(fields[10], out->selected_player_name, sizeof(out->selected_player_name));
                out->selected_player_score = kbo_fa_compensation_parse_i32(fields[11]);
                out->unprotected_candidate_count = (int)kbo_fa_compensation_parse_u32(fields[12]);
                out->cash_with_player = kbo_fa_compensation_parse_u32(fields[13]);
                out->cash_only = kbo_fa_compensation_parse_u32(fields[14]);
                kbo_fa_compensation_copy_token(fields[15], out->source, sizeof(out->source));
                found = out->selected_player_id != 0u || strcmp(out->action, "CASH_ONLY") == 0;
            }
        }
        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

