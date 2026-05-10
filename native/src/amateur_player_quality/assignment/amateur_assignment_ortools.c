#include "../internal/amateur_player_quality_internal.h"

static int kbo_amateur_ortools_get_script_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    HMODULE module = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&kbo_amateur_ortools_get_script_path,
            &module)) {
        return 0;
    }

    char module_path[MAX_PATH * 3] = {0};
    DWORD len = GetModuleFileNameA(module, module_path, (DWORD)sizeof(module_path));
    if (len == 0 || len >= sizeof(module_path)) {
        return 0;
    }
    char* slash = strrchr(module_path, '\\');
    if (slash == NULL) {
        return 0;
    }
    slash[1] = '\0';
    snprintf(out, out_size, "%stools\\amateur_assignment_optimizer.py", module_path);
    return GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES;
}

static int kbo_amateur_ortools_write_request(
    const char* path,
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    int player_tier,
    int32_t target_reputation,
    KboAmateurAssignmentCandidate* candidates,
    int count)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    int32_t target_max_players = kbo_amateur_assignment_target_max_players(league_id);
    fprintf(
        file,
        "player_id,league_id,age,quality_score,player_tier,target_reputation,current_team_id,current_reputation,target_max_players,team_id,reputation,team_tier,player_count,hitter_count,rejected\r\n");

    for (int i = 0; i < count; i++) {
        int team_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
        int rejected = kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id);
        fprintf(
            file,
            "%u,%u,%d,%d,%d,%d,%u,%u,%d,%u,%u,%d,%d,%d,%d\r\n",
            player_id,
            league_id,
            (int)age,
            quality_score,
            player_tier,
            target_reputation,
            current_team_id,
            (uint32_t)current_reputation,
            target_max_players,
            candidates[i].team_id,
            (uint32_t)candidates[i].reputation,
            team_tier,
            candidates[i].player_count,
            candidates[i].hitter_count,
            rejected);
    }

    fclose(file);
    return 1;
}

static int kbo_amateur_ortools_run(const char* script_path, const char* request_path, const char* result_path)
{
    DeleteFileA(result_path);

    char command[MAX_PATH * 9] = {0};
    snprintf(
        command,
        sizeof(command),
        "python \"%s\" \"%s\" \"%s\"",
        script_path,
        request_path,
        result_path);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        static volatile LONG create_fail_count = 0;
        if (InterlockedIncrement(&create_fail_count) <= 5) {
            append_logf("amateur OR-Tools optimizer launch failed gle=%lu script=%s", GetLastError(), script_path);
        }
        return 0;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, 5000);
    DWORD exit_code = 1;
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        append_log_line("amateur OR-Tools optimizer timed out");
    } else {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return wait != WAIT_TIMEOUT && exit_code == 0 && GetFileAttributesA(result_path) != INVALID_FILE_ATTRIBUTES;
}

static uint32_t kbo_amateur_ortools_read_result(const char* result_path)
{
    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0u;
    }
    char line[512] = {0};
    if (fgets(line, sizeof(line), file) == NULL || fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0u;
    }
    fclose(file);
    return (uint32_t)strtoul(line, NULL, 10);
}

uint8_t* kbo_choose_amateur_assignment_team_ortools(
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    uint8_t* out_target_reputation)
{
    if (player == NULL || league_id == 0u || current_team_id == 0u || quality_score <= 0) {
        return NULL;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1 || candidates == NULL) {
        return NULL;
    }

    int32_t target = kbo_amateur_assignment_target_reputation(league_id, quality_score);
    if (out_target_reputation != NULL) {
        *out_target_reputation = (uint8_t)target;
    }
    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);
    if (player_tier > 0 && target <= (int32_t)current_reputation) {
        return NULL;
    }

    char script_path[MAX_PATH * 3] = {0};
    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_amateur_ortools_get_script_path(script_path, sizeof(script_path))
            || !kbo_get_save_scoped_data_file("amateur_assignment_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("amateur_assignment_ortools_result.csv", result_path, sizeof(result_path))) {
        return NULL;
    }

    if (!kbo_amateur_ortools_write_request(
            request_path,
            player,
            league_id,
            current_team_id,
            current_reputation,
            quality_score,
            player_tier,
            target,
            candidates,
            count)) {
        return NULL;
    }
    if (!kbo_amateur_ortools_run(script_path, request_path, result_path)) {
        return NULL;
    }

    uint32_t target_team_id = kbo_amateur_ortools_read_result(result_path);
    if (target_team_id == 0u || target_team_id == current_team_id) {
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == target_team_id) {
            static volatile LONG log_count = 0;
            LONG slot = InterlockedIncrement(&log_count);
            if (slot <= 50 || read_kbo_localappdata_flag_file("enable_amateur_assignment_verbose_log.txt")) {
                uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                append_logf(
                    "amateur OR-Tools assignment player=%u league=%u score=%d target_rep=%d team=%u(rep=%u)->%u(rep=%u)",
                    player_id,
                    league_id,
                    quality_score,
                    target,
                    current_team_id,
                    (uint32_t)current_reputation,
                    target_team_id,
                    (uint32_t)candidates[i].reputation);
            }
            return candidates[i].team;
        }
    }
    return NULL;
}
