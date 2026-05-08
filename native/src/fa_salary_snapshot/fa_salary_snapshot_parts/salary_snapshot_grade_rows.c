#include "salary_snapshot_grade_rows.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "salary_snapshot_csv_parse.h"
#include "salary_snapshot_paths_dates.h"

static const char* kbo_fa_salary_snapshot_grade_for_ranks(uint32_t overall_rank, uint32_t team_rank, int32_t salary)
{
    if (salary <= 0 || (overall_rank == 0u && team_rank == 0u)) {
        return "UNKNOWN";
    }
    if ((overall_rank != 0u && overall_rank <= 30u) || (team_rank != 0u && team_rank <= 3u)) {
        return "A";
    }
    if ((overall_rank != 0u && overall_rank <= 60u) || (team_rank != 0u && team_rank <= 10u)) {
        return "B";
    }
    return "C";
}

static int __cdecl kbo_fa_salary_snapshot_grade_compare_overall(const void* a, const void* b)
{
    const KboFaSalarySnapshotGrade* left = (const KboFaSalarySnapshotGrade*)a;
    const KboFaSalarySnapshotGrade* right = (const KboFaSalarySnapshotGrade*)b;

    int left_has_salary = left->salary > 0 && !left->foreign_flag;
    int right_has_salary = right->salary > 0 && !right->foreign_flag;
    if (left_has_salary != right_has_salary) {
        return right_has_salary - left_has_salary;
    }
    if (left->salary != right->salary) {
        return right->salary > left->salary ? 1 : -1;
    }
    if (left->ranking_team_id != right->ranking_team_id) {
        return left->ranking_team_id < right->ranking_team_id ? -1 : 1;
    }
    if (left->player_id != right->player_id) {
        return left->player_id < right->player_id ? -1 : 1;
    }
    return 0;
}

static int __cdecl kbo_fa_salary_snapshot_grade_compare_team(const void* a, const void* b)
{
    const KboFaSalarySnapshotGrade* left = (const KboFaSalarySnapshotGrade*)a;
    const KboFaSalarySnapshotGrade* right = (const KboFaSalarySnapshotGrade*)b;

    if (left->ranking_team_id != right->ranking_team_id) {
        return left->ranking_team_id < right->ranking_team_id ? -1 : 1;
    }

    int left_has_salary = left->salary > 0 && !left->foreign_flag;
    int right_has_salary = right->salary > 0 && !right->foreign_flag;
    if (left_has_salary != right_has_salary) {
        return right_has_salary - left_has_salary;
    }
    if (left->salary != right->salary) {
        return right->salary > left->salary ? 1 : -1;
    }
    if (left->player_id != right->player_id) {
        return left->player_id < right->player_id ? -1 : 1;
    }
    return 0;
}

static void kbo_fa_salary_snapshot_assign_grade_overall_ranks(KboFaSalarySnapshotGrade* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotGrade), kbo_fa_salary_snapshot_grade_compare_overall);

    uint32_t ordinal = 0u;
    uint32_t current_rank = 0u;
    int32_t last_salary = -1;
    for (int i = 0; i < count; i++) {
        rows[i].overall_rank = 0u;
        rows[i].overall_ordinal = 0u;
        if (rows[i].foreign_flag || rows[i].salary <= 0) {
            continue;
        }

        ordinal++;
        if (rows[i].salary != last_salary) {
            current_rank = ordinal;
            last_salary = rows[i].salary;
        }
        rows[i].overall_rank = current_rank;
        rows[i].overall_ordinal = ordinal;
    }
}

static void kbo_fa_salary_snapshot_assign_grade_team_ranks(KboFaSalarySnapshotGrade* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotGrade), kbo_fa_salary_snapshot_grade_compare_team);

    uint32_t current_team = 0u;
    uint32_t ordinal = 0u;
    uint32_t current_rank = 0u;
    int32_t last_salary = -1;
    for (int i = 0; i < count; i++) {
        rows[i].team_rank = 0u;
        rows[i].team_ordinal = 0u;
        if (rows[i].ranking_team_id == 0u || rows[i].foreign_flag || rows[i].salary <= 0) {
            continue;
        }

        if (rows[i].ranking_team_id != current_team) {
            current_team = rows[i].ranking_team_id;
            ordinal = 0u;
            current_rank = 0u;
            last_salary = -1;
        }

        ordinal++;
        if (rows[i].salary != last_salary) {
            current_rank = ordinal;
            last_salary = rows[i].salary;
        }
        rows[i].team_rank = current_rank;
        rows[i].team_ordinal = ordinal;
    }
}

int kbo_fa_salary_snapshot_load_grade_rows(
    uint32_t season,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || max_rows <= 0 || season < 1982u || season > 2200u) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_fa_salary_snapshot_path(season, path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 8u * 1024u * 1024u) {
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

    int count = 0;
    char* cursor = buffer;
    while (*cursor != '\0' && count < max_rows) {
        char* next = strchr(cursor, '\n');
        if (next != NULL) {
            *next = '\0';
        }

        char* p = cursor;
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            KboFaSalarySnapshotGrade grade_row;
            memset(&grade_row, 0, sizeof(grade_row));
            for (int field_index = 0; field_index <= 21 && *p != '\0'; field_index++) {
                char field[128] = {0};
                if (!kbo_fa_salary_snapshot_parse_csv_field(&p, field, sizeof(field))) {
                    break;
                }

                switch (field_index) {
                case 0: grade_row.snapshot_date = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 1: grade_row.season = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 2: grade_row.opening_day = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 5: grade_row.player_id = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 10: grade_row.ranking_team_id = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 16: grade_row.foreign_flag = kbo_fa_salary_snapshot_parse_u32(field) != 0u ? 1u : 0u; break;
                case 17: grade_row.salary = kbo_fa_salary_snapshot_parse_i32(field); break;
                case 18: grade_row.overall_rank = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 19: grade_row.overall_ordinal = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 20: grade_row.team_rank = kbo_fa_salary_snapshot_parse_u32(field); break;
                case 21: grade_row.team_ordinal = kbo_fa_salary_snapshot_parse_u32(field); break;
                default: break;
                }
            }

            if (grade_row.player_id != 0u && grade_row.season == season) {
                rows[count++] = grade_row;
            }
        }

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    kbo_fa_salary_snapshot_assign_grade_overall_ranks(rows, count);
    kbo_fa_salary_snapshot_assign_grade_team_ranks(rows, count);
    for (int i = 0; i < count; i++) {
        snprintf(
            rows[i].grade,
            sizeof(rows[i].grade),
            "%s",
            kbo_fa_salary_snapshot_grade_for_ranks(
                rows[i].overall_rank,
                rows[i].team_rank,
                rows[i].salary));
    }
    return count;
}

const KboFaSalarySnapshotGrade* kbo_find_fa_salary_snapshot_grade(
    const KboFaSalarySnapshotGrade* rows,
    int row_count,
    uint32_t player_id)
{
    if (rows == NULL || row_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < row_count; i++) {
        if (rows[i].player_id == player_id) {
            return &rows[i];
        }
    }
    return NULL;
}
