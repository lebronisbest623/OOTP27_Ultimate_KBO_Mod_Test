#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../config/foreign_waiver_config.h"
#include "../paths/foreign_waiver_paths.h"
#include "../policy/foreign_player_policy.h"
#include "foreign_waiver_player_eval.h"

int16_t kbo_read_player_i16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || offset + sizeof(int16_t) > OOTP27_PLAYER_SCAN_BYTES
            || !memory_range_readable(player + offset, sizeof(int16_t))) {
        return 0;
    }
    return *(int16_t*)(player + offset);
}

int32_t kbo_foreign_waiver_value_score(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);

    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    return talent * policy->value_score_talent_weight
        + overall * policy->value_score_overall_weight
        + ratings * policy->value_score_ratings_weight
        + career * policy->value_score_career_weight;
}

int32_t kbo_get_foreign_waiver_value_threshold(void)
{
    uint32_t configured = read_u32_leading_number_from_file("foreign_waiver_value_threshold.txt");
    if (configured != 0u && configured <= 250000u) {
        return (int32_t)configured;
    }
    return kbo_foreign_player_policy()->regular_value_threshold;
}

int32_t kbo_get_foreign_waiver_asian_value_threshold(void)
{
    uint32_t configured = read_u32_leading_number_from_file("foreign_waiver_asian_value_threshold.txt");
    if (configured != 0u && configured <= 250000u) {
        return (int32_t)configured;
    }
    return kbo_foreign_player_policy()->asian_value_threshold;
}

int32_t kbo_get_foreign_waiver_value_threshold_for_player(uint8_t* player)
{
    if (player != NULL && kbo_player_is_asian_quota_candidate(player)) {
        return kbo_get_foreign_waiver_asian_value_threshold();
    }
    return kbo_get_foreign_waiver_value_threshold();
}

uint8_t* kbo_find_player_by_id(uint32_t player_id, uint32_t* out_current_team_id, uint32_t* out_current_league_id)
{
    if (player_id == 0) {
        return NULL;
    }

    if (out_current_team_id != NULL) { *out_current_team_id = 0; }
    if (out_current_league_id != NULL) { *out_current_league_id = 0; }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t candidate_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (candidate_id != player_id) {
            continue;
        }

        if (out_current_team_id != NULL) {
            *out_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        }
        if (out_current_league_id != NULL) {
            *out_current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        }
        return player;
    }
    return NULL;
}

int kbo_player_is_foreign_for_kbo_rights(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    return nation_id != 0u && nation_id != OOTP27_KBO_KOREA_NATION_ID;
}

#define KBO_ASIAN_QUOTA_NATION_MAX 64

static uint32_t g_kbo_asian_quota_nation_ids[KBO_ASIAN_QUOTA_NATION_MAX] = {0};
static LONG g_kbo_asian_quota_nation_count = -1;

static void kbo_add_asian_quota_nation_id(uint32_t nation_id, int* count)
{
    if (count == NULL || *count >= KBO_ASIAN_QUOTA_NATION_MAX
            || nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        if (g_kbo_asian_quota_nation_ids[i] == nation_id) {
            return;
        }
    }
    g_kbo_asian_quota_nation_ids[(*count)++] = nation_id;
}

int kbo_load_asian_quota_nation_ids_once(void)
{
    LONG cached = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -1, -1);
    if (cached >= 0) {
        return (int)cached;
    }

    LONG marker = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -2, -1);
    if (marker != -1) {
        while ((cached = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -2, -2)) == -2) {
            SwitchToThread();
        }
        return cached > 0 ? (int)cached : 0;
    }

    int count = 0;
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    for (int i = 0; i < policy->asian_quota_nation_count; i++) {
        kbo_add_asian_quota_nation_id(policy->asian_quota_nation_ids[i], &count);
    }

    char path[MAX_PATH] = {0};
    if (get_kbo_asian_quota_nation_ids_path(path, sizeof(path))) {
        HANDLE file = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file != INVALID_HANDLE_VALUE) {
            char raw[4096] = {0};
            DWORD read = 0;
            if (ReadFile(file, raw, sizeof(raw) - 1, &read, NULL) && read > 0) {
                raw[read < sizeof(raw) ? read : sizeof(raw) - 1] = '\0';
                char* cursor = raw;
                while (*cursor != '\0' && count < KBO_ASIAN_QUOTA_NATION_MAX) {
                    while (*cursor != '\0' && (*cursor < '0' || *cursor > '9')) {
                        cursor++;
                    }
                    if (*cursor == '\0') {
                        break;
                    }
                    char* tail = cursor;
                    unsigned long value = strtoul(cursor, &tail, 10);
                    if (tail == cursor) {
                        break;
                    }
                    if (value > 0ul && value <= 1000000ul) {
                        kbo_add_asian_quota_nation_id((uint32_t)value, &count);
                    }
                    cursor = tail;
                }
            }
            CloseHandle(file);
        }
    }

    kbo_log_runtimef("asian quota: loaded nation_ids=%d path=%s", count, path);
    InterlockedExchange(&g_kbo_asian_quota_nation_count, (LONG)count);
    return count;
}

int kbo_nation_is_asian_quota_candidate(uint32_t nation_id)
{
    if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }

    int count = kbo_load_asian_quota_nation_ids_once();
    for (int i = 0; i < count; i++) {
        if (g_kbo_asian_quota_nation_ids[i] == nation_id) {
            return 1;
        }
    }
    return 0;
}

static int32_t kbo_player_asian_quota_salary(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(int32_t))
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))) {
        return 0;
    }

    int32_t y1_salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    if (y1_salary > 0) {
        return y1_salary;
    }

    if (memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))) {
        int32_t demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
        if (demand > 0) {
            return demand;
        }
    }

    for (uint32_t i = 1; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
        if (salary > 0) {
            return salary;
        }
    }

    return 0;
}

#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2

int kbo_player_is_asian_quota_candidate(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (!kbo_nation_is_asian_quota_candidate(*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET))) {
        return 0;
    }
    int32_t salary = kbo_player_asian_quota_salary(player);
    return salary > 0 && salary <= kbo_get_asian_quota_salary_limit();
}

