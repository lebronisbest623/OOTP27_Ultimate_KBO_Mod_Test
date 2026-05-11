#include "salary_snapshot_write_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../competitive_balance_tax/api/competitive_balance_tax.h"
#include "../../core/logging/core_log.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../csv/salary_snapshot_csv_parse.h"
#include "../paths/salary_snapshot_paths_dates.h"
#include "../ranking/salary_snapshot_row_ranking.h"

int kbo_fa_salary_snapshot_write_csv(
    const KboFaSalarySnapshotRow* rows,
    int row_count,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_salary_snapshot_path(season, path, sizeof(path))) {
        append_logf("KBO FA salary snapshot skipped source=%s reason=path_unavailable season=%u", source != NULL ? source : "", season);
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA salary snapshot failed open path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    DWORD written = 0;
    const char* header =
        "date,season,opening_day,source,league_id,player_id,name,nation_id,current_team_id,active_team_id,ranking_team_id,current_league_id,draft_league_id,age,retired_flag,contract_level,foreign_flag,salary,overall_rank,overall_ordinal,team_rank,team_ordinal,contract_status,contract_start_year,contract_y1,contract_y2,contract_y3,contract_y4,contract_y5,contract_y6,contract_y7,contract_y8,contract_y9,contract_y10\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < row_count; i++) {
        const KboFaSalarySnapshotRow* row = &rows[i];
        char prefix[512] = {0};
        int prefix_len = snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,",
            date,
            season,
            opening_day);
        if (prefix_len > 0) {
            WriteFile(file, prefix, (DWORD)prefix_len, &written, NULL);
        }
        kbo_fa_salary_snapshot_write_csv_text(file, source != NULL ? source : "");
        snprintf(
            prefix,
            sizeof(prefix),
            ",%u,%u,",
            league_id,
            row->player_id);
        WriteFile(file, prefix, (DWORD)strlen(prefix), &written, NULL);
        kbo_fa_salary_snapshot_write_csv_text(file, row->player_name);
        snprintf(
            prefix,
            sizeof(prefix),
            ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%d,%d",
            row->nation_id,
            row->current_team_id,
            row->active_team_id,
            row->ranking_team_id,
            row->current_league_id,
            row->draft_league_id,
            (uint32_t)row->age,
            (uint32_t)row->retired_flag,
            (uint32_t)row->contract_level,
            (uint32_t)row->foreign_flag,
            row->salary,
            row->overall_rank,
            row->overall_ordinal,
            row->team_rank,
            row->team_ordinal,
            row->contract_status,
            row->contract_start_year);
        WriteFile(file, prefix, (DWORD)strlen(prefix), &written, NULL);
        for (uint32_t y = 0; y < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; y++) {
            char salary_text[32] = {0};
            snprintf(salary_text, sizeof(salary_text), ",%d", row->contract_years[y]);
            WriteFile(file, salary_text, (DWORD)strlen(salary_text), &written, NULL);
        }
        WriteFile(file, "\r\n", 2, &written, NULL);
    }

    CloseHandle(file);
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}

int kbo_capture_fa_salary_opening_day_snapshot(const char* source, uint32_t date, uint32_t season, uint32_t opening_day, uint32_t league_id)
{
    LARGE_INTEGER profile_snapshot_capture = {0};
    int profile_snapshot_capture_active = kbo_profiler_begin(&profile_snapshot_capture);

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        append_logf("KBO FA salary snapshot skipped source=%s reason=no_player_vector date=%u", source != NULL ? source : "", date);
        if (profile_snapshot_capture_active) {
            kbo_profiler_end("fa_salary_snapshot.capture.no_player_vector", &profile_snapshot_capture);
        }
        return 0;
    }

    KboFaSalarySnapshotRow* rows = (KboFaSalarySnapshotRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)player_count * sizeof(KboFaSalarySnapshotRow));
    if (rows == NULL) {
        append_logf("KBO FA salary snapshot skipped source=%s reason=alloc_failed count=%d", source != NULL ? source : "", player_count);
        if (profile_snapshot_capture_active) {
            kbo_profiler_end("fa_salary_snapshot.capture.alloc_failed", &profile_snapshot_capture);
        }
        return 0;
    }

    int row_count = 0;
    int scanned = 0;
    int salary_rows = 0;
    int zero_salary_rows = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t ranking_team_id = kbo_fa_salary_snapshot_resolve_ranking_team(league_id, current_team_id, active_team_id);
        if (ranking_team_id == 0u) {
            continue;
        }

        uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
        if (retired_flag != 0u || age < 16u || age > 60u) {
            continue;
        }

        KboFaSalarySnapshotRow* row = &rows[row_count++];
        row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row->current_team_id = current_team_id;
        row->active_team_id = active_team_id;
        row->ranking_team_id = ranking_team_id;
        row->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        row->draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
        row->age = age;
        row->retired_flag = retired_flag;
        row->contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        row->foreign_flag = row->nation_id != OOTP27_KBO_KOREA_NATION_ID ? 1u : 0u;
        row->salary = kbo_fa_salary_snapshot_player_salary_for_season(player, season);
        kbo_fa_salary_snapshot_copy_contract_years(player, row);
        kbo_copy_player_display_name(player, row->player_name, sizeof(row->player_name));
        if (row->player_name[0] == '\0' || strcmp(row->player_name, "Unknown player") == 0) {
            snprintf(row->player_name, sizeof(row->player_name), "Player #%u", row->player_id);
        }

        if (row->salary > 0) {
            salary_rows++;
        } else {
            zero_salary_rows++;
        }
    }

    kbo_fa_salary_snapshot_assign_overall_ranks(rows, row_count);
    kbo_fa_salary_snapshot_assign_team_ranks(rows, row_count);
    qsort(rows, (size_t)row_count, sizeof(KboFaSalarySnapshotRow), kbo_fa_salary_snapshot_compare_overall);

    char path[MAX_PATH] = {0};
    int wrote = kbo_fa_salary_snapshot_write_csv(rows, row_count, date, season, opening_day, league_id, source, path, sizeof(path));
    if (wrote) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, (LONG)season);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, 1);
        kbo_process_competitive_balance_tax(season, source);
        append_logf(
            "KBO FA salary opening-day snapshot written source=%s date=%u season=%u opening_day=%u league=%u scanned=%d rows=%d salary_rows=%d zero_salary_rows=%d csv=%s",
            source != NULL ? source : "",
            date,
            season,
            opening_day,
            league_id,
            scanned,
            row_count,
            salary_rows,
            zero_salary_rows,
            path);
    }

    HeapFree(GetProcessHeap(), 0, rows);
    if (profile_snapshot_capture_active) {
        kbo_profiler_end("fa_salary_snapshot.capture", &profile_snapshot_capture);
    }
    return wrote;
}
