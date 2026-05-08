/* Asian Games schedule seed loading. Included from native/src/custom_events.inc. */

#define KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX 96

typedef struct KboAsianGamesScheduleSeed {
    uint32_t year;
    char host_city[48];
    char host_country[48];
    char status[24];
    uint32_t tournament_start;
    uint32_t tournament_end;
    uint32_t selection_date;
    uint32_t departure_date;
    uint32_t final_date;
    uint8_t auto_schedule;
    char notes[128];
} KboAsianGamesScheduleSeed;

static KboAsianGamesScheduleSeed g_kbo_asian_games_schedule_seeds[KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX];
static int g_kbo_asian_games_schedule_seed_count = 0;
static LONG g_kbo_asian_games_schedule_seed_lock = 0;
static LONG g_kbo_asian_games_schedule_seed_loaded = 0;
static char g_kbo_asian_games_schedule_seed_loaded_key[MAX_PATH * 3] = {0};

static void kbo_lock_asian_games_schedule_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_asian_games_schedule_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_asian_games_schedule_seeds(void)
{
    InterlockedExchange(&g_kbo_asian_games_schedule_seed_lock, 0);
}

static int kbo_get_save_asian_games_schedule_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("asian_games_schedule_seed.csv", out, out_size);
}

static int kbo_get_global_asian_games_schedule_seed_path(char* out, size_t out_size)
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
