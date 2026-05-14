#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "cbt_draft_order_ledger.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/logging/core_log.h"

#define KBO_CBT_DRAFT_ORDER_LEDGER_FILE "cbt_draft_order_penalties.csv"

static int kbo_cbt_draft_order_ledger_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(KBO_CBT_DRAFT_ORDER_LEDGER_FILE, out, out_size);
}

static void kbo_cbt_draft_order_copy_csv_text(const char* in, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (in == NULL) {
        return;
    }

    size_t used = 0u;
    for (const char* p = in; *p != '\0' && used + 1u < out_size; p++) {
        char ch = *p;
        if (ch == ',' || ch == '\r' || ch == '\n') {
            ch = ' ';
        }
        out[used++] = ch;
    }
    out[used] = '\0';
}

int kbo_cbt_draft_order_append_ledger(const KboCbtDraftOrderMove* move, const char* source)
{
    if (move == NULL || move->season == 0u || move->team_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_draft_order_ledger_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "KBO CBT draft order ledger skipped season=%u team=%u reason=path_unavailable",
            move->season,
            move->team_id);
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ | FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "KBO CBT draft order ledger open failed season=%u team=%u gle=%lu path=%s",
            move->season,
            move->team_id,
            (unsigned long)GetLastError(),
            path);
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    int needs_header = (size == 0u && high == 0u);
    DWORD written = 0u;
    if (needs_header) {
        const char* header =
            "season,team_id,round,stages,from_pick,to_pick,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char safe_source[128] = {0};
    kbo_cbt_draft_order_copy_csv_text(source != NULL ? source : "", safe_source, sizeof(safe_source));

    char line[256] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%u,%u,%u,%u,%u,%u,%s\r\n",
        move->season,
        move->team_id,
        KBO_CBT_DRAFT_ORDER_TARGET_ROUND,
        move->stages,
        move->from_pick,
        move->to_pick,
        safe_source);
    if (len <= 0) {
        CloseHandle(file);
        return 0;
    }

    int ok = WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len;
    CloseHandle(file);
    return ok;
}
