#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/core_flags/api/flags_api.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/optimizer/kbo_optimizer.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../decisions/fa_compensation_decisions.h"
#include "../protection/fa_compensation_protection_score.h"
#include "fa_compensation_due_ortools.h"

#define KBO_FA_COMPENSATION_ORTOOLS_SCORE_WEIGHT 1000
#define KBO_FA_COMPENSATION_ORTOOLS_ROLE_BONUS 25

static int kbo_fa_compensation_write_ortools_request(
    const char* path,
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidates,
    int candidate_count)
{
    if (path == NULL || rec == NULL || candidates == NULL || candidate_count <= 0) {
        return 0;
    }
    char tmp_path[MAX_PATH] = {0};
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE* file = fopen(tmp_path, "wb");
    if (file == NULL) {
        return 0;
    }
    fputs("player_id,score,decision_score,age,role,protect_count\r\n", file);
    for (int i = 0; i < candidate_count; i++) {
        char reason[96] = {0};
        int32_t decision_score = kbo_fa_compensation_player_decision_score(
            rec,
            &candidates[i],
            reason,
            sizeof(reason));
        fprintf(
            file,
            "%u,%d,%d,%u,%u,%u\r\n",
            candidates[i].player_id,
            candidates[i].score,
            decision_score,
            (uint32_t)candidates[i].age,
            (uint32_t)candidates[i].role,
            rec->protect_count);
    }
    fclose(file);
    if (!MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp_path);
        return 0;
    }
    return 1;
}

static int kbo_fa_compensation_candidate_index_by_player_id(
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const uint8_t* used,
    uint32_t player_id)
{
    if (candidates == NULL || used == NULL || player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < candidate_count; i++) {
        if (!used[i] && candidates[i].player_id == player_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_fa_compensation_order_has_unique_score_frontier(
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    uint32_t protect_count)
{
    if (candidates == NULL || candidate_count <= 0 || protect_count == 0u) {
        return 0;
    }
    if (protect_count >= (uint32_t)candidate_count) {
        return 1;
    }
    for (int i = 1; i < candidate_count; i++) {
        if (candidates[i - 1].score < candidates[i].score) {
            return 0;
        }
    }

    uint8_t seen_roles[256] = {0};
    int distinct_roles = 0;
    for (int i = 0; i < candidate_count; i++) {
        uint8_t role = candidates[i].role;
        if (!seen_roles[role]) {
            seen_roles[role] = 1u;
            distinct_roles++;
        }
    }
    if (distinct_roles * KBO_FA_COMPENSATION_ORTOOLS_ROLE_BONUS >= KBO_FA_COMPENSATION_ORTOOLS_SCORE_WEIGHT) {
        return 0;
    }

    int32_t boundary_score = candidates[protect_count - 1u].score;
    int above_boundary = 0;
    int at_boundary = 0;
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].score > boundary_score) {
            above_boundary++;
        } else if (candidates[i].score == boundary_score) {
            at_boundary++;
        }
    }
    int needed_at_boundary = (int)protect_count - above_boundary;
    return needed_at_boundary == at_boundary;
}

int kbo_fa_compensation_apply_ortools_order(
    KboFaCompensationRecord* rec,
    KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source)
{
    if (read_kbo_localappdata_flag_file("disable_fa_compensation_ortools.txt")
            || rec == NULL || candidates == NULL || candidate_count <= 0 || rec->protect_count == 0u) {
        return 0;
    }
    if (kbo_fa_compensation_order_has_unique_score_frontier(candidates, candidate_count, rec->protect_count)) {
        kbo_profiler_record_us("fa_compensation.ortools.skip_unique_frontier", 0);
        return 1;
    }

    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_get_save_scoped_data_file("fa_compensation_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("fa_compensation_ortools_result.csv", result_path, sizeof(result_path))) {
        return 0;
    }
    if (!kbo_fa_compensation_write_ortools_request(request_path, rec, candidates, candidate_count)) {
        return 0;
    }
    if (!kbo_optimizer_run_mode("fa_compensation", request_path, result_path, 8000u)) {
        return 0;
    }

    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0;
    }
    char line[256] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    KboFaProtectedCandidate ordered[KBO_FA_COMPENSATION_PROTECTED_LIST_MAX];
    uint8_t used[KBO_FA_COMPENSATION_PROTECTED_LIST_MAX] = {0};
    memset(ordered, 0, sizeof(ordered));
    int ordered_count = 0;
    while (ordered_count < candidate_count && fgets(line, sizeof(line), file) != NULL) {
        uint32_t player_id = (uint32_t)strtoul(line, NULL, 10);
        int index = kbo_fa_compensation_candidate_index_by_player_id(
            candidates,
            candidate_count,
            used,
            player_id);
        if (index < 0) {
            continue;
        }
        ordered[ordered_count++] = candidates[index];
        used[index] = 1u;
    }
    fclose(file);
    if (ordered_count <= 0) {
        return 0;
    }

    for (int i = 0; i < candidate_count && ordered_count < candidate_count; i++) {
        if (!used[i]) {
            ordered[ordered_count++] = candidates[i];
        }
    }
    memcpy(candidates, ordered, (SIZE_T)candidate_count * sizeof(candidates[0]));
    kbo_log_runtimef(
        "KBO FA compensation OR-Tools protected order applied source=%s fa_player=%u signing_team=%u candidates=%d protect=%u",
        source != NULL ? source : "",
        rec->player_id,
        rec->signing_team_id,
        candidate_count,
        rec->protect_count);
    return 1;
}
