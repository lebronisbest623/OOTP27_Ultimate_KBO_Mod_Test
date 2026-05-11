#include "../internal/amateur_player_quality_internal.h"

typedef struct KboAmateurBatchAssignment {
    uint32_t player_id;
    uint32_t league_id;
    uint32_t target_team_id;
} KboAmateurBatchAssignment;

#define KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX 4096
#define KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX 256
#define KBO_AMATEUR_LEAGUE_BATCH_IDLE_MS 10000u
#define KBO_AMATEUR_LEAGUE_BATCH_NEAR_COMPLETE_IDLE_MS 1500u
#define KBO_AMATEUR_LEAGUE_BATCH_NEAR_COMPLETE_TEAMS 96
#define KBO_AMATEUR_LEAGUE_BATCH_NEAR_COMPLETE_PLAYERS 3000

typedef struct KboAmateurDeferredTeamAdd {
    uintptr_t team_ptr;
    uintptr_t player_ptr;
    uintptr_t arg3;
    uintptr_t arg4;
    uintptr_t arg5;
    uintptr_t arg6;
    uintptr_t arg7;
    uintptr_t arg8;
    uint32_t caller_rva;
    uint32_t league_id;
    uint32_t source_team_id;
    uint32_t player_id;
} KboAmateurDeferredTeamAdd;

static KboAmateurBatchAssignment g_kbo_amateur_batch_assignments[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
static LONG g_kbo_amateur_batch_assignment_count = 0;
static LONG g_kbo_amateur_batch_assignment_lock = 0;
static uintptr_t g_kbo_amateur_league_batch_players[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
static uintptr_t g_kbo_amateur_league_batch_source_teams[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
static uint32_t g_kbo_amateur_league_batch_team_ids[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
static uint32_t g_kbo_amateur_league_batch_league_id = 0u;
static int32_t g_kbo_amateur_league_batch_player_count = 0;
static int32_t g_kbo_amateur_league_batch_team_count = 0;
static DWORD g_kbo_amateur_league_batch_last_tick = 0u;
static volatile LONG g_kbo_amateur_league_batch_flush_thread_started = 0;
static KboAmateurDeferredTeamAdd g_kbo_amateur_deferred_team_adds[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
static int32_t g_kbo_amateur_deferred_team_add_count = 0;
static volatile LONG g_kbo_amateur_disable_reroute_cached = -1;
static volatile LONG g_kbo_amateur_disable_reroute_tick = 0;
static volatile LONG g_kbo_amateur_disable_deferred_cached = -1;
static volatile LONG g_kbo_amateur_disable_deferred_tick = 0;
static volatile LONG g_kbo_amateur_verbose_log_cached = -1;
static volatile LONG g_kbo_amateur_verbose_log_tick = 0;

static int kbo_amateur_cached_bool_flag(
    const char* file_name,
    volatile LONG* cached_value,
    volatile LONG* cached_tick,
    DWORD ttl_ms)
{
    DWORD now = GetTickCount();
    LONG value = *cached_value;
    LONG tick = *cached_tick;
    if (value >= 0 && now - (DWORD)tick < ttl_ms) {
        return value != 0;
    }

    int fresh = read_kbo_localappdata_flag_file(file_name) ? 1 : 0;
    InterlockedExchange(cached_value, fresh);
    InterlockedExchange(cached_tick, (LONG)now);
    return fresh;
}

static int kbo_amateur_reroute_disabled_cached(void)
{
    return kbo_amateur_cached_bool_flag(
        "disable_amateur_assignment_reroute.txt",
        &g_kbo_amateur_disable_reroute_cached,
        &g_kbo_amateur_disable_reroute_tick,
        1000u);
}

static int kbo_amateur_deferred_add_disabled_cached(void)
{
    return kbo_amateur_cached_bool_flag(
        "disable_amateur_assignment_deferred_add.txt",
        &g_kbo_amateur_disable_deferred_cached,
        &g_kbo_amateur_disable_deferred_tick,
        1000u);
}

static int kbo_amateur_verbose_log_enabled_cached(void)
{
    return kbo_amateur_cached_bool_flag(
        "enable_amateur_assignment_verbose_log.txt",
        &g_kbo_amateur_verbose_log_cached,
        &g_kbo_amateur_verbose_log_tick,
        5000u);
}

static void kbo_amateur_batch_lock(void)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_batch_assignment_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_amateur_batch_unlock(void)
{
    InterlockedExchange(&g_kbo_amateur_batch_assignment_lock, 0);
}

static uint32_t kbo_amateur_batch_lookup(uint32_t league_id, uint32_t player_id)
{
    uint32_t target_team_id = 0u;
    kbo_amateur_batch_lock();
    LONG count = g_kbo_amateur_batch_assignment_count;
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_amateur_batch_assignments[i].league_id == league_id
                && g_kbo_amateur_batch_assignments[i].player_id == player_id) {
            target_team_id = g_kbo_amateur_batch_assignments[i].target_team_id;
            break;
        }
    }
    kbo_amateur_batch_unlock();
    return target_team_id;
}

static int kbo_amateur_ortools_get_tool_path(char* out, size_t out_size, int* out_is_python_script)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (out_is_python_script != NULL) {
        *out_is_python_script = 0;
    }

    HMODULE module = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&kbo_amateur_ortools_get_tool_path,
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

    snprintf(out, out_size, "%stools\\kbo_optimizer.exe", module_path);
    if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) {
        return 1;
    }

    snprintf(out, out_size, "%stools\\kbo_optimizer.py", module_path);
    if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) {
        if (out_is_python_script != NULL) {
            *out_is_python_script = 1;
        }
        return 1;
    }

    snprintf(out, out_size, "%stools\\amateur_assignment_optimizer.exe", module_path);
    if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) {
        return 1;
    }

    snprintf(out, out_size, "%stools\\amateur_assignment_optimizer.py", module_path);
    int exists = GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES;
    if (exists && out_is_python_script != NULL) {
        *out_is_python_script = 1;
    }
    if (!exists) {
        static volatile LONG missing_log_count = 0;
        if (InterlockedIncrement(&missing_log_count) <= 5) {
            append_logf(
                "amateur OR-Tools optimizer missing exe=%stools\\kbo_optimizer.exe script=%stools\\kbo_optimizer.py",
                module_path,
                module_path);
        }
    }
    return exists;
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

static uint32_t kbo_amateur_batch_resolve_source_team_id(uint8_t* player, uintptr_t source_team_ptr)
{
    uint32_t current_team_id = 0u;
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        current_team_id = kbo_amateur_player_assignment_team_id(player);
    }
    if (source_team_ptr != 0
            && memory_range_readable((void*)source_team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        uint32_t source_team_id = *(uint32_t*)((uint8_t*)source_team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        if (current_team_id == 0u) {
            current_team_id = source_team_id;
        }
    }
    return current_team_id;
}

static int kbo_amateur_batch_source_index(uint32_t* team_ids, int count, uint32_t team_id)
{
    if (team_id == 0u) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (team_ids[i] == team_id) {
            return i;
        }
    }
    return -1;
}

static uintptr_t kbo_amateur_candidate_team_ptr_by_id(
    KboAmateurAssignmentCandidate* candidates,
    int count,
    uint32_t team_id)
{
    if (candidates == NULL || count <= 0 || team_id == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            return (uintptr_t)candidates[i].team;
        }
    }
    return 0;
}

static int kbo_amateur_ortools_write_batch_request(
    const char* path,
    uintptr_t* players,
    uintptr_t* source_teams,
    int32_t player_count,
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int count,
    int incoming_batch)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }

    int32_t target_max_players = kbo_amateur_assignment_target_max_players(league_id);
    uint32_t source_team_ids[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
    int32_t source_player_counts[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
    int32_t source_hitter_counts[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
    int32_t source_position_counts[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX][KBO_AMATEUR_POSITION_BUCKET_COUNT];
    int source_team_count = 0;
    memset(source_team_ids, 0, sizeof(source_team_ids));
    memset(source_player_counts, 0, sizeof(source_player_counts));
    memset(source_hitter_counts, 0, sizeof(source_hitter_counts));
    memset(source_position_counts, 0, sizeof(source_position_counts));

    for (int32_t p = 0; p < player_count; p++) {
        uint8_t* player = (uint8_t*)players[p];
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (player_id == 0u || !kbo_amateur_player_age_eligible(league_id, age)) {
            continue;
        }
        uint32_t source_team_id = kbo_amateur_batch_resolve_source_team_id(
            player,
            source_teams != NULL ? source_teams[p] : 0);
        if (source_team_id == 0u) {
            continue;
        }
        int source_index = kbo_amateur_batch_source_index(source_team_ids, source_team_count, source_team_id);
        if (source_index < 0) {
            if (source_team_count >= KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX) {
                continue;
            }
            source_index = source_team_count++;
            source_team_ids[source_index] = source_team_id;
        }
        source_player_counts[source_index]++;
        if (kbo_amateur_player_is_hitter(player)) {
            source_hitter_counts[source_index]++;
        }
        int position_bucket = kbo_amateur_player_position_bucket(player);
        if (position_bucket >= 0 && position_bucket < KBO_AMATEUR_POSITION_BUCKET_COUNT) {
            source_position_counts[source_index][position_bucket]++;
        }
    }

    fprintf(
        file,
        "player_id,league_id,age,quality_score,player_tier,target_reputation,current_team_id,current_reputation,is_hitter,role_bucket,position_group,position_role,source_batch_count,source_hitter_batch_count,source_pitcher_batch_count,source_catcher_batch_count,source_infielder_batch_count,source_outfielder_batch_count,source_first_base_batch_count,source_second_base_batch_count,source_third_base_batch_count,source_shortstop_batch_count,source_left_field_batch_count,source_center_field_batch_count,source_right_field_batch_count,source_designated_hitter_batch_count,target_max_players,team_id,reputation,team_tier,player_count,hitter_count,pitcher_count,catcher_count,infielder_count,outfielder_count,first_base_count,second_base_count,third_base_count,shortstop_count,left_field_count,center_field_count,right_field_count,designated_hitter_count,rejected,batch_mode\r\n");

    int written_players = 0;
    for (int32_t p = 0; p < player_count; p++) {
        uint8_t* player = (uint8_t*)players[p];
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        int32_t quality_score = kbo_amateur_quality_score(player);
        if (player_id == 0u || quality_score <= 0 || !kbo_amateur_player_age_eligible(league_id, age)) {
            continue;
        }

        uint32_t current_team_id = kbo_amateur_batch_resolve_source_team_id(
            player,
            source_teams != NULL ? source_teams[p] : 0);

        uint8_t current_reputation = 0u;
        for (int i = 0; i < count; i++) {
            if (candidates[i].team_id == current_team_id) {
                current_reputation = candidates[i].reputation;
                break;
            }
        }
        if (current_team_id == 0u || current_reputation == 0u) {
            continue;
        }

        int32_t target = kbo_amateur_assignment_target_reputation(league_id, quality_score);
        int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);

        int source_index = kbo_amateur_batch_source_index(source_team_ids, source_team_count, current_team_id);
        int32_t source_batch_count = source_index >= 0 ? source_player_counts[source_index] : 0;
        int32_t source_hitter_batch_count = source_index >= 0 ? source_hitter_counts[source_index] : 0;
        int32_t source_pitcher_batch_count = source_index >= 0 ? source_position_counts[source_index][0] : 0;
        int32_t source_catcher_batch_count = source_index >= 0 ? source_position_counts[source_index][1] : 0;
        int32_t source_first_base_batch_count = source_index >= 0 ? source_position_counts[source_index][2] : 0;
        int32_t source_second_base_batch_count = source_index >= 0 ? source_position_counts[source_index][3] : 0;
        int32_t source_third_base_batch_count = source_index >= 0 ? source_position_counts[source_index][4] : 0;
        int32_t source_shortstop_batch_count = source_index >= 0 ? source_position_counts[source_index][5] : 0;
        int32_t source_left_field_batch_count = source_index >= 0 ? source_position_counts[source_index][6] : 0;
        int32_t source_center_field_batch_count = source_index >= 0 ? source_position_counts[source_index][7] : 0;
        int32_t source_right_field_batch_count = source_index >= 0 ? source_position_counts[source_index][8] : 0;
        int32_t source_designated_hitter_batch_count = source_index >= 0 ? source_position_counts[source_index][9] : 0;
        int32_t source_infielder_batch_count = source_first_base_batch_count
            + source_second_base_batch_count
            + source_third_base_batch_count
            + source_shortstop_batch_count
            + source_designated_hitter_batch_count;
        int32_t source_outfielder_batch_count = source_left_field_batch_count
            + source_center_field_batch_count
            + source_right_field_batch_count;
        int is_hitter = kbo_amateur_player_is_hitter(player);
        uint8_t position_group = memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))
            ? player[OOTP27_PLAYER_POSITION_GROUP_OFFSET]
            : 0u;
        uint8_t position_role = memory_range_readable(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET, sizeof(uint8_t))
            ? player[OOTP27_PLAYER_POSITION_ROLE_OFFSET]
            : 0u;
        const char* role_bucket = kbo_amateur_position_bucket_label(kbo_amateur_player_position_bucket(player));
        int wrote_player = 0;
        for (int i = 0; i < count; i++) {
            int team_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
            int rejected = kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id);
            if (rejected) {
                continue;
            }
            if (!wrote_player) {
                written_players++;
                wrote_player = 1;
            }
            fprintf(
                file,
                "%u,%u,%d,%d,%d,%d,%u,%u,%d,%s,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\r\n",
                player_id,
                league_id,
                (int)age,
                quality_score,
                player_tier,
                target,
                current_team_id,
                (uint32_t)current_reputation,
                is_hitter,
                role_bucket,
                (uint32_t)position_group,
                (uint32_t)position_role,
                source_batch_count,
                source_hitter_batch_count,
                source_pitcher_batch_count,
                source_catcher_batch_count,
                source_infielder_batch_count,
                source_outfielder_batch_count,
                source_first_base_batch_count,
                source_second_base_batch_count,
                source_third_base_batch_count,
                source_shortstop_batch_count,
                source_left_field_batch_count,
                source_center_field_batch_count,
                source_right_field_batch_count,
                source_designated_hitter_batch_count,
                target_max_players,
                candidates[i].team_id,
                (uint32_t)candidates[i].reputation,
                team_tier,
                candidates[i].player_count,
                candidates[i].hitter_count,
                candidates[i].pitcher_count,
                candidates[i].catcher_count,
                candidates[i].infielder_count,
                candidates[i].outfielder_count,
                candidates[i].first_base_count,
                candidates[i].second_base_count,
                candidates[i].third_base_count,
                candidates[i].shortstop_count,
                candidates[i].left_field_count,
                candidates[i].center_field_count,
                candidates[i].right_field_count,
                candidates[i].designated_hitter_count,
                rejected,
                incoming_batch ? "incoming" : "roster");
        }
    }

    fclose(file);
    return written_players > 0;
}

static int kbo_amateur_league_batch_has_team(uint32_t team_id)
{
    for (int32_t i = 0; i < g_kbo_amateur_league_batch_team_count; i++) {
        if (g_kbo_amateur_league_batch_team_ids[i] == team_id) {
            return 1;
        }
    }
    return 0;
}

static int32_t kbo_amateur_league_batch_find_player_index(uint32_t player_id)
{
    for (int32_t i = 0; i < g_kbo_amateur_league_batch_player_count; i++) {
        uint8_t* player = (uint8_t*)g_kbo_amateur_league_batch_players[i];
        if (player != NULL
                && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                && *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_amateur_league_batch_has_player(uint32_t player_id)
{
    return kbo_amateur_league_batch_find_player_index(player_id) >= 0;
}

static int kbo_amateur_local_player_list_has_id(uintptr_t* players, int32_t count, uint32_t player_id)
{
    for (int32_t i = 0; i < count; i++) {
        uint8_t* player = (uint8_t*)players[i];
        if (player != NULL
                && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                && *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_amateur_generation_team_add_caller(uint32_t caller_rva)
{
    switch (caller_rva) {
    case 0x00A30BA0u:
    case 0x00A3105Bu:
    case 0x00A312A7u:
    case 0x00A3150Fu:
    case 0x00A3175Bu:
    case 0x00A319A8u:
        return 1;
    default:
        return 0;
    }
}

static void kbo_amateur_league_batch_clear(uint32_t league_id)
{
    memset(g_kbo_amateur_league_batch_players, 0, sizeof(g_kbo_amateur_league_batch_players));
    memset(g_kbo_amateur_league_batch_source_teams, 0, sizeof(g_kbo_amateur_league_batch_source_teams));
    memset(g_kbo_amateur_league_batch_team_ids, 0, sizeof(g_kbo_amateur_league_batch_team_ids));
    memset(g_kbo_amateur_deferred_team_adds, 0, sizeof(g_kbo_amateur_deferred_team_adds));
    g_kbo_amateur_league_batch_league_id = league_id;
    g_kbo_amateur_league_batch_player_count = 0;
    g_kbo_amateur_league_batch_team_count = 0;
    g_kbo_amateur_deferred_team_add_count = 0;
    g_kbo_amateur_league_batch_last_tick = 0u;
}

static PROCESS_INFORMATION g_kbo_amateur_ortools_worker_pi;
static HANDLE g_kbo_amateur_ortools_worker_stdin = NULL;
static HANDLE g_kbo_amateur_ortools_worker_stdout = NULL;
static LONG g_kbo_amateur_ortools_worker_lock = 0;

static void kbo_amateur_ortools_close_worker(void)
{
    if (g_kbo_amateur_ortools_worker_stdin != NULL) {
        CloseHandle(g_kbo_amateur_ortools_worker_stdin);
        g_kbo_amateur_ortools_worker_stdin = NULL;
    }
    if (g_kbo_amateur_ortools_worker_stdout != NULL) {
        CloseHandle(g_kbo_amateur_ortools_worker_stdout);
        g_kbo_amateur_ortools_worker_stdout = NULL;
    }
    if (g_kbo_amateur_ortools_worker_pi.hProcess != NULL) {
        TerminateProcess(g_kbo_amateur_ortools_worker_pi.hProcess, 1);
        CloseHandle(g_kbo_amateur_ortools_worker_pi.hProcess);
        g_kbo_amateur_ortools_worker_pi.hProcess = NULL;
    }
    if (g_kbo_amateur_ortools_worker_pi.hThread != NULL) {
        CloseHandle(g_kbo_amateur_ortools_worker_pi.hThread);
        g_kbo_amateur_ortools_worker_pi.hThread = NULL;
    }
}

static int kbo_amateur_ortools_worker_running(void)
{
    if (g_kbo_amateur_ortools_worker_pi.hProcess == NULL
            || g_kbo_amateur_ortools_worker_stdin == NULL
            || g_kbo_amateur_ortools_worker_stdout == NULL) {
        return 0;
    }
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(g_kbo_amateur_ortools_worker_pi.hProcess, &exit_code)) {
        return 0;
    }
    return exit_code == STILL_ACTIVE;
}

static int kbo_amateur_ortools_start_worker(const char* tool_path)
{
    if (kbo_amateur_ortools_worker_running()) {
        return 1;
    }
    kbo_amateur_ortools_close_worker();

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdin_read = NULL;
    HANDLE stdin_write = NULL;
    HANDLE stdout_read = NULL;
    HANDLE stdout_write = NULL;
    HANDLE stderr_nul = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
        return 0;
    }
    if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return 0;
    }
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return 0;
    }
    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        return 0;
    }

    char command[MAX_PATH * 4] = {0};
    snprintf(command, sizeof(command), "\"%s\" --server", tool_path);

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    memset(&g_kbo_amateur_ortools_worker_pi, 0, sizeof(g_kbo_amateur_ortools_worker_pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    stderr_nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    si.hStdError = stderr_nul != INVALID_HANDLE_VALUE ? stderr_nul : stdout_write;
    si.wShowWindow = SW_HIDE;

    int ok = CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &g_kbo_amateur_ortools_worker_pi);
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    if (stderr_nul != INVALID_HANDLE_VALUE) {
        CloseHandle(stderr_nul);
    }
    if (!ok) {
        static volatile LONG create_fail_count = 0;
        if (InterlockedIncrement(&create_fail_count) <= 5) {
            append_logf("amateur OR-Tools worker launch failed gle=%lu tool=%s", GetLastError(), tool_path);
        }
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        return 0;
    }

    g_kbo_amateur_ortools_worker_stdin = stdin_write;
    g_kbo_amateur_ortools_worker_stdout = stdout_read;
    append_logf("amateur OR-Tools worker started tool=%s", tool_path);
    return 1;
}

static int kbo_amateur_ortools_run_worker(const char* tool_path, const char* request_path, const char* result_path)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_ortools_worker_lock, 1, 0) != 0) {
        Sleep(0);
    }

    int ok = 0;
    do {
        DeleteFileA(result_path);
        if (!kbo_amateur_ortools_start_worker(tool_path)) {
            break;
        }

        for (int attempt = 0; attempt < 2 && !ok; attempt++) {
            char line[MAX_PATH * 6] = {0};
            snprintf(line, sizeof(line), "%s\t%s\n", request_path, result_path);
            DWORD written = 0;
            if (!WriteFile(g_kbo_amateur_ortools_worker_stdin, line, (DWORD)strlen(line), &written, NULL)) {
                kbo_amateur_ortools_close_worker();
                break;
            }
            FlushFileBuffers(g_kbo_amateur_ortools_worker_stdin);

            char response[128] = {0};
            DWORD used = 0;
            DWORD start = GetTickCount();
            while (used + 1 < sizeof(response)) {
                DWORD available = 0;
                if (!PeekNamedPipe(g_kbo_amateur_ortools_worker_stdout, NULL, 0, NULL, &available, NULL)) {
                    kbo_amateur_ortools_close_worker();
                    break;
                }
                if (available == 0) {
                    if (GetTickCount() - start > 5000u) {
                        append_log_line("amateur OR-Tools worker timed out");
                        kbo_amateur_ortools_close_worker();
                        break;
                    }
                    Sleep(1);
                    continue;
                }
                char ch = '\0';
                DWORD read = 0;
                if (!ReadFile(g_kbo_amateur_ortools_worker_stdout, &ch, 1, &read, NULL) || read != 1) {
                    kbo_amateur_ortools_close_worker();
                    break;
                }
                if (ch == '\n') {
                    ok = strncmp(response, "OK 0", 4) == 0 && GetFileAttributesA(result_path) != INVALID_FILE_ATTRIBUTES;
                    break;
                }
                if (ch != '\r') {
                    response[used++] = ch;
                }
            }
            if (!ok && response[0] != '\0') {
                static volatile LONG fail_log_count = 0;
                if (InterlockedIncrement(&fail_log_count) <= 5) {
                    append_logf("amateur OR-Tools worker response=%s", response);
                }
            }
        }
    } while (0);

    InterlockedExchange(&g_kbo_amateur_ortools_worker_lock, 0);
    return ok;
}

static int kbo_amateur_ortools_run(const char* tool_path, int is_python_script, const char* request_path, const char* result_path)
{
    DeleteFileA(result_path);

    if (!is_python_script) {
        return kbo_amateur_ortools_run_worker(tool_path, request_path, result_path);
    }

    char command[MAX_PATH * 9] = {0};
    snprintf(
        command,
        sizeof(command),
        "python \"%s\" \"%s\" \"%s\"",
        tool_path,
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
            append_logf("amateur OR-Tools optimizer launch failed gle=%lu tool=%s", GetLastError(), tool_path);
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

static int kbo_amateur_ortools_read_batch_result(const char* result_path, uint32_t league_id)
{
    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0;
    }

    char line[512] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    KboAmateurBatchAssignment assignments[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    int count = 0;
    while (count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX && fgets(line, sizeof(line), file) != NULL) {
        char* cursor = line;
        uint32_t player_id = (uint32_t)strtoul(cursor, &cursor, 10);
        if (*cursor == ',') {
            cursor++;
        }
        uint32_t target_team_id = (uint32_t)strtoul(cursor, NULL, 10);
        if (player_id != 0u && target_team_id != 0u) {
            assignments[count].player_id = player_id;
            assignments[count].league_id = league_id;
            assignments[count].target_team_id = target_team_id;
            count++;
        }
    }
    fclose(file);

    kbo_amateur_batch_lock();
    memset(g_kbo_amateur_batch_assignments, 0, sizeof(g_kbo_amateur_batch_assignments));
    memcpy(g_kbo_amateur_batch_assignments, assignments, (size_t)count * sizeof(assignments[0]));
    InterlockedExchange(&g_kbo_amateur_batch_assignment_count, count);
    kbo_amateur_batch_unlock();
    return count;
}

static void kbo_amateur_apply_deferred_original_fallback(
    KboAmateurDeferredTeamAdd* deferred_team_adds,
    int32_t deferred_count,
    uint32_t league_id,
    const char* reason)
{
    int applied = 0;
    int skipped_cross_league = 0;
    for (int32_t i = 0; i < deferred_count; i++) {
        KboAmateurDeferredTeamAdd* item = &deferred_team_adds[i];
        if (kbo_resolve_amateur_assignment_league_id_for_team_and_player(
                (uint8_t*)item->team_ptr,
                (uint8_t*)item->player_ptr) != league_id) {
            skipped_cross_league++;
            continue;
        }
        uint8_t result = kbo_team_add_player_guard_call_original(
            item->team_ptr,
            item->player_ptr,
            item->arg3,
            item->arg4,
            item->arg5,
            item->arg6,
            item->arg7,
            item->arg8);
        if (result != 0u) {
            applied++;
            kbo_amateur_team_add_player_note_original_success(
                item->team_ptr,
                item->player_ptr,
                "deferred_original_fallback",
                result);
        }
    }
    if (deferred_count > 0) {
        append_logf(
            "amateur deferred team-add fallback applied league=%u applied=%d/%d skipped_cross_league=%d reason=%s",
            league_id,
            applied,
            deferred_count,
            skipped_cross_league,
            reason != NULL ? reason : "");
    }
}

static int kbo_amateur_flush_league_batch_ortools(const char* reason, int force)
{
    uint32_t league_id = 0u;
    int32_t accumulated_players = 0;
    int32_t optimizer_player_count = 0;
    int32_t accumulated_teams = 0;
    int32_t deferred_count = 0;
    uintptr_t league_players[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    uintptr_t league_source_teams[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    KboAmateurDeferredTeamAdd deferred_team_adds[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];

    kbo_amateur_batch_lock();
    league_id = g_kbo_amateur_league_batch_league_id;
    accumulated_players = g_kbo_amateur_league_batch_player_count;
    accumulated_teams = g_kbo_amateur_league_batch_team_count;
    deferred_count = g_kbo_amateur_deferred_team_add_count;
    DWORD idle_ms = (accumulated_teams >= KBO_AMATEUR_LEAGUE_BATCH_NEAR_COMPLETE_TEAMS
            || accumulated_players >= KBO_AMATEUR_LEAGUE_BATCH_NEAR_COMPLETE_PLAYERS)
        ? KBO_AMATEUR_LEAGUE_BATCH_NEAR_COMPLETE_IDLE_MS
        : KBO_AMATEUR_LEAGUE_BATCH_IDLE_MS;
    if ((league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID)
            || accumulated_players <= 1
            || accumulated_teams <= 0
            || (!force && GetTickCount() - g_kbo_amateur_league_batch_last_tick < idle_ms)) {
        kbo_amateur_batch_unlock();
        return 0;
    }
    memcpy(league_players, g_kbo_amateur_league_batch_players, (size_t)accumulated_players * sizeof(uintptr_t));
    memcpy(league_source_teams, g_kbo_amateur_league_batch_source_teams, (size_t)accumulated_players * sizeof(uintptr_t));
    memcpy(deferred_team_adds, g_kbo_amateur_deferred_team_adds, (size_t)deferred_count * sizeof(deferred_team_adds[0]));
    kbo_amateur_league_batch_clear(league_id);
    kbo_amateur_batch_unlock();

    optimizer_player_count = accumulated_players;
    if (deferred_count > 0) {
        memset(league_players, 0, sizeof(league_players));
        memset(league_source_teams, 0, sizeof(league_source_teams));
        optimizer_player_count = 0;
        for (int32_t i = 0; i < deferred_count && optimizer_player_count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX; i++) {
            KboAmateurDeferredTeamAdd* item = &deferred_team_adds[i];
            if (item->league_id != league_id || item->player_id == 0u || item->player_ptr == 0 || item->team_ptr == 0) {
                continue;
            }
            if (kbo_amateur_local_player_list_has_id(league_players, optimizer_player_count, item->player_id)) {
                continue;
            }
            league_players[optimizer_player_count] = item->player_ptr;
            league_source_teams[optimizer_player_count] = item->team_ptr;
            optimizer_player_count++;
        }
    }
    if (optimizer_player_count <= 1) {
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "no_optimizer_players");
        return 0;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1 || candidates == NULL) {
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "no_candidates");
        return 0;
    }

    char tool_path[MAX_PATH * 3] = {0};
    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    int is_python_script = 0;
    if (!kbo_amateur_ortools_get_tool_path(tool_path, sizeof(tool_path), &is_python_script)
            || !kbo_get_save_scoped_data_file("amateur_assignment_ortools_batch_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("amateur_assignment_ortools_batch_result.csv", result_path, sizeof(result_path))) {
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "path_unavailable");
        return 0;
    }

    if (!kbo_amateur_ortools_write_batch_request(
            request_path,
            league_players,
            league_source_teams,
            optimizer_player_count,
            league_id,
            candidates,
            count,
            deferred_count > 0)) {
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "write_failed");
        return 0;
    }
    if (!kbo_amateur_ortools_run(tool_path, is_python_script, request_path, result_path)) {
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "ortools_failed");
        return 0;
    }
    int assigned = kbo_amateur_ortools_read_batch_result(result_path, league_id);
    append_logf(
        "amateur OR-Tools league batch prepared league=%u teams=%d/%d players=%d assignments=%d deferred=%d reason=%s",
        league_id,
        accumulated_teams,
        count,
        optimizer_player_count,
        assigned,
        deferred_count,
        reason != NULL ? reason : "");
    int applied = 0;
    int fallback_applied = 0;
    int target_not_found = 0;
    int target_add_failed = 0;
    int source_retry_applied = 0;
    int still_failed = 0;
    for (int32_t i = 0; i < deferred_count; i++) {
        KboAmateurDeferredTeamAdd* item = &deferred_team_adds[i];
        uintptr_t source_team_ptr = kbo_amateur_candidate_team_ptr_by_id(candidates, count, item->source_team_id);
        if (source_team_ptr == 0) {
            if (kbo_resolve_amateur_assignment_league_id_for_team_and_player(
                    (uint8_t*)item->team_ptr,
                    (uint8_t*)item->player_ptr) != item->league_id) {
                still_failed++;
                continue;
            }
            source_team_ptr = item->team_ptr;
        }
        uintptr_t target_team_ptr = source_team_ptr;
        uint32_t target_team_id = kbo_amateur_batch_lookup(item->league_id, item->player_id);
        int target_found = 1;
        if (target_team_id != 0u && target_team_id != item->source_team_id) {
            target_found = 0;
            for (int c = 0; c < count; c++) {
                if (candidates[c].team_id == target_team_id) {
                    target_team_ptr = (uintptr_t)candidates[c].team;
                    target_found = 1;
                    break;
                }
            }
            if (!target_found) {
                target_not_found++;
                fallback_applied++;
                target_team_ptr = source_team_ptr;
            }
        } else {
            fallback_applied++;
        }
        uint8_t result = kbo_team_add_player_guard_call_original(
            target_team_ptr,
            item->player_ptr,
            item->arg3,
            item->arg4,
            item->arg5,
            item->arg6,
            item->arg7,
            item->arg8);
        if (result == 0u && target_team_ptr != source_team_ptr) {
            target_add_failed++;
            result = kbo_team_add_player_guard_call_original(
                source_team_ptr,
                item->player_ptr,
                item->arg3,
                item->arg4,
                item->arg5,
                item->arg6,
                item->arg7,
                item->arg8);
            if (result != 0u) {
                source_retry_applied++;
                target_team_ptr = source_team_ptr;
            }
        }
        if (result == 0u
                && source_team_ptr != item->team_ptr
                && kbo_resolve_amateur_assignment_league_id_for_team_and_player(
                    (uint8_t*)item->team_ptr,
                    (uint8_t*)item->player_ptr) == item->league_id) {
            result = kbo_team_add_player_guard_call_original(
                item->team_ptr,
                item->player_ptr,
                item->arg3,
                item->arg4,
                item->arg5,
                item->arg6,
                item->arg7,
                item->arg8);
            if (result != 0u) {
                source_retry_applied++;
                target_team_ptr = item->team_ptr;
            }
        }
        if (result != 0u) {
            applied++;
            kbo_amateur_team_add_player_note_original_success(
                target_team_ptr,
                item->player_ptr,
                target_team_ptr == source_team_ptr ? "deferred_original_success" : "deferred_ortools_success",
                result);
        } else {
            still_failed++;
            static volatile LONG failed_log_count = 0;
            LONG slot = InterlockedIncrement(&failed_log_count);
            if (slot <= 20 || kbo_amateur_verbose_log_enabled_cached()) {
                append_logf(
                    "amateur deferred team-add failed player=%u source_team=%u target_team=%u target_found=%d",
                    item->player_id,
                    item->source_team_id,
                    target_team_id,
                    target_found);
            }
        }
    }
    if (deferred_count > 0) {
        append_logf(
            "amateur deferred team-add batch applied league=%u applied=%d/%d fallback_original=%d target_not_found=%d target_add_failed=%d source_retry_applied=%d still_failed=%d",
            league_id,
            applied,
            deferred_count,
            fallback_applied,
            target_not_found,
            target_add_failed,
            source_retry_applied,
            still_failed);
    }
    return assigned;
}

static DWORD WINAPI kbo_amateur_league_batch_flush_thread(LPVOID parameter)
{
    (void)parameter;
    for (;;) {
        Sleep(250);
        if (kbo_amateur_reroute_disabled_cached()) {
            break;
        }
        if (kbo_amateur_flush_league_batch_ortools("idle", 0)) {
            break;
        }
        kbo_amateur_batch_lock();
        int empty = g_kbo_amateur_league_batch_player_count <= 0;
        kbo_amateur_batch_unlock();
        if (empty) {
            break;
        }
    }
    InterlockedExchange(&g_kbo_amateur_league_batch_flush_thread_started, 0);
    return 0;
}

static void kbo_amateur_start_league_batch_flush_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_amateur_league_batch_flush_thread_started, 1, 0) != 0) {
        return;
    }
    HANDLE thread = CreateThread(NULL, 0, kbo_amateur_league_batch_flush_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_kbo_amateur_league_batch_flush_thread_started, 0);
        append_logf("amateur OR-Tools league batch flush thread failed gle=%lu", GetLastError());
    }
}

int kbo_amateur_defer_team_add_if_generation(
    uint32_t caller_rva,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8)
{
    if (!kbo_amateur_generation_team_add_caller(caller_rva)
            || kbo_amateur_reroute_disabled_cached()
            || kbo_amateur_deferred_add_disabled_cached()
            || team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return 0;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_league_id = kbo_amateur_player_assignment_league_id(player);
    uint32_t team_league_id = kbo_resolve_amateur_assignment_league_id_for_team_and_player(team, player);
    uint32_t league_id =
        (player_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || player_league_id == KBO_COLLEGE_LEAGUE_ID)
            ? player_league_id
            : team_league_id;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t source_team_id = kbo_amateur_player_assignment_team_id(player);
    if (source_team_id == 0u) {
        source_team_id = team_id;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if ((league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID)
            || source_team_id == 0u
            || player_id == 0u
            || !kbo_amateur_player_age_eligible(league_id, age)
            || kbo_player_is_draft_pool_candidate(player)) {
        return 0;
    }

    kbo_amateur_batch_lock();
    if (g_kbo_amateur_league_batch_league_id != 0u
            && g_kbo_amateur_league_batch_league_id != league_id
            && (g_kbo_amateur_league_batch_player_count > 0
                || g_kbo_amateur_deferred_team_add_count > 0)) {
        kbo_amateur_batch_unlock();
        kbo_amateur_flush_league_batch_ortools("league_switch", 1);
        kbo_amateur_batch_lock();
    }
    if (g_kbo_amateur_league_batch_league_id != league_id
            || g_kbo_amateur_league_batch_player_count >= KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX
            || g_kbo_amateur_deferred_team_add_count >= KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX) {
        kbo_amateur_league_batch_clear(league_id);
    }

    if (!kbo_amateur_league_batch_has_team(source_team_id)
            && g_kbo_amateur_league_batch_team_count < KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX) {
        g_kbo_amateur_league_batch_team_ids[g_kbo_amateur_league_batch_team_count++] = source_team_id;
    }

    int32_t existing_player_index = kbo_amateur_league_batch_find_player_index(player_id);
    if (existing_player_index >= 0) {
        if (g_kbo_amateur_league_batch_source_teams[existing_player_index] == 0) {
            g_kbo_amateur_league_batch_source_teams[existing_player_index] = team_ptr;
        }
    } else if (g_kbo_amateur_league_batch_player_count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX) {
        int32_t index = g_kbo_amateur_league_batch_player_count++;
        g_kbo_amateur_league_batch_players[index] = player_ptr;
        g_kbo_amateur_league_batch_source_teams[index] = team_ptr;
    }

    KboAmateurDeferredTeamAdd* item = &g_kbo_amateur_deferred_team_adds[g_kbo_amateur_deferred_team_add_count++];
    item->team_ptr = team_ptr;
    item->player_ptr = player_ptr;
    item->arg3 = arg3;
    item->arg4 = arg4;
    item->arg5 = arg5;
    item->arg6 = arg6;
    item->arg7 = arg7;
    item->arg8 = arg8;
    item->caller_rva = caller_rva;
    item->league_id = league_id;
    item->source_team_id = source_team_id;
    item->player_id = player_id;

    g_kbo_amateur_league_batch_last_tick = GetTickCount();
    int32_t queued = g_kbo_amateur_deferred_team_add_count;
    int32_t teams = g_kbo_amateur_league_batch_team_count;
    kbo_amateur_batch_unlock();

    static volatile LONG defer_log_count = 0;
    LONG slot = InterlockedIncrement(&defer_log_count);
    if (slot <= 80 || kbo_amateur_verbose_log_enabled_cached()) {
        append_logf(
            "amateur deferred team-add queued caller_rva=0x%x league=%u team=%u source_team=%u player=%u queued=%d teams=%d",
            caller_rva,
            league_id,
            team_id,
            source_team_id,
            player_id,
            queued,
            teams);
    }
    kbo_amateur_start_league_batch_flush_thread();
    return 1;
}

void kbo_prepare_amateur_assignment_batch_ortools(uintptr_t player_list_ptr, int32_t player_count, uintptr_t source_team_ptr)
{
    (void)source_team_ptr;
    if (player_list_ptr == 0 || player_count <= 1 || player_count > 512) {
        return;
    }
    if (!memory_range_readable((void*)player_list_ptr, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return;
    }

    uintptr_t* players = (uintptr_t*)player_list_ptr;
    uint32_t league_id = 0u;
    uint32_t source_team_id = 0u;
    for (int32_t i = 0; i < player_count; i++) {
        uint8_t* player = (uint8_t*)players[i];
        if (player == NULL) {
            continue;
        }
        uint32_t candidate_league_id = kbo_amateur_player_assignment_league_id(player);
        if (candidate_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || candidate_league_id == KBO_COLLEGE_LEAGUE_ID) {
            league_id = candidate_league_id;
            source_team_id = kbo_amateur_player_assignment_team_id(player);
            break;
        }
    }
    if (league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID) {
        return;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1 || candidates == NULL) {
        return;
    }

    kbo_amateur_batch_lock();
    if (g_kbo_amateur_league_batch_league_id != 0u
            && g_kbo_amateur_league_batch_league_id != league_id
            && (g_kbo_amateur_league_batch_player_count > 0
                || g_kbo_amateur_deferred_team_add_count > 0)) {
        kbo_amateur_batch_unlock();
        kbo_amateur_flush_league_batch_ortools("league_switch", 1);
        kbo_amateur_batch_lock();
    }
    if (g_kbo_amateur_league_batch_league_id == league_id
            && g_kbo_amateur_deferred_team_add_count > 0) {
        g_kbo_amateur_league_batch_last_tick = GetTickCount();
        int32_t accumulated_players = g_kbo_amateur_league_batch_player_count;
        int32_t accumulated_teams = g_kbo_amateur_league_batch_team_count;
        int ready = accumulated_teams >= count || accumulated_teams >= KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX;
        if (!ready) {
            kbo_amateur_batch_unlock();
            kbo_amateur_start_league_batch_flush_thread();
            return;
        }
        kbo_amateur_batch_unlock();
        (void)accumulated_players;
        (void)accumulated_teams;
        kbo_amateur_flush_league_batch_ortools("team_count", 1);
        return;
    }
    if (source_team_id == 0u) {
        kbo_amateur_batch_unlock();
        return;
    }

    if (g_kbo_amateur_league_batch_league_id != league_id
            || g_kbo_amateur_league_batch_player_count >= KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX
            || (kbo_amateur_league_batch_has_team(source_team_id)
                && g_kbo_amateur_league_batch_team_count >= count)) {
        kbo_amateur_league_batch_clear(league_id);
    }

    if (!kbo_amateur_league_batch_has_team(source_team_id)
            && g_kbo_amateur_league_batch_team_count < KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX) {
        g_kbo_amateur_league_batch_team_ids[g_kbo_amateur_league_batch_team_count++] = source_team_id;
    }

    for (int32_t i = 0; i < player_count && g_kbo_amateur_league_batch_player_count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX; i++) {
        uint8_t* player = (uint8_t*)players[i];
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || kbo_amateur_player_assignment_league_id(player) != league_id) {
            continue;
        }
        if (!kbo_amateur_league_batch_has_player(player_id)) {
            int32_t index = g_kbo_amateur_league_batch_player_count++;
            g_kbo_amateur_league_batch_players[index] = (uintptr_t)player;
            g_kbo_amateur_league_batch_source_teams[index] = 0;
        }
    }

    g_kbo_amateur_league_batch_last_tick = GetTickCount();
    int32_t accumulated_players = g_kbo_amateur_league_batch_player_count;
    int32_t accumulated_teams = g_kbo_amateur_league_batch_team_count;
    int ready = accumulated_teams >= count || accumulated_teams >= KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX;
    if (!ready) {
        append_logf(
            "amateur OR-Tools league batch accumulating league=%u teams=%d/%d players=%d latest_team=%u latest_players=%d",
            league_id,
            accumulated_teams,
            count,
            accumulated_players,
            source_team_id,
            player_count);
        kbo_amateur_batch_unlock();
        kbo_amateur_start_league_batch_flush_thread();
        return;
    }
    kbo_amateur_batch_unlock();
    (void)accumulated_players;
    (void)accumulated_teams;
    kbo_amateur_flush_league_batch_ortools("team_count", 1);
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

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t batch_target_team_id = kbo_amateur_batch_lookup(league_id, player_id);
    if (batch_target_team_id != 0u && batch_target_team_id != current_team_id) {
        for (int i = 0; i < count; i++) {
            if (candidates[i].team_id == batch_target_team_id) {
                if (out_target_reputation != NULL) {
                    *out_target_reputation = candidates[i].reputation;
                }
                return candidates[i].team;
            }
        }
    }

    int32_t target = kbo_amateur_assignment_target_reputation(league_id, quality_score);
    if (out_target_reputation != NULL) {
        *out_target_reputation = (uint8_t)target;
    }
    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);

    char tool_path[MAX_PATH * 3] = {0};
    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    int is_python_script = 0;
    if (!kbo_amateur_ortools_get_tool_path(tool_path, sizeof(tool_path), &is_python_script)) {
        return NULL;
    }
    if (!kbo_get_save_scoped_data_file("amateur_assignment_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("amateur_assignment_ortools_result.csv", result_path, sizeof(result_path))) {
        static volatile LONG path_fail_log_count = 0;
        if (InterlockedIncrement(&path_fail_log_count) <= 5) {
            append_log_line("amateur OR-Tools save-scoped request/result path unavailable");
        }
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
    if (!kbo_amateur_ortools_run(tool_path, is_python_script, request_path, result_path)) {
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
            if (slot <= 50 || kbo_amateur_verbose_log_enabled_cached()) {
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
