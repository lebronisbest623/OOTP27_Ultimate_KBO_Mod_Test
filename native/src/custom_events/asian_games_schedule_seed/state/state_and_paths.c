#include "../../runtime/common/custom_events_common.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include "state_and_paths.h"
#include "../../../core/files/save_paths/core_save_paths.h"

KboAsianGamesScheduleSeed g_kbo_asian_games_schedule_seeds[KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX];
int g_kbo_asian_games_schedule_seed_count = 0;
static LONG g_kbo_asian_games_schedule_seed_lock = 0;
LONG g_kbo_asian_games_schedule_seed_loaded = 0;
char g_kbo_asian_games_schedule_seed_loaded_key[MAX_PATH * 3] = {0};

void kbo_lock_asian_games_schedule_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_asian_games_schedule_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

void kbo_unlock_asian_games_schedule_seeds(void)
{
    InterlockedExchange(&g_kbo_asian_games_schedule_seed_lock, 0);
}

int kbo_get_save_asian_games_schedule_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("asian_games_schedule_seed.csv", out, out_size);
}

int kbo_get_global_asian_games_schedule_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\asian_games_schedule_seed.csv", local_app_data);
    return out[0] != '\0';
}
