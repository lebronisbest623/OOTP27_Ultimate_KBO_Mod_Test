#include "../internal/captain_selection_internal.h"

volatile LONG g_kbo_captain_preseason_thread_started = 0;
volatile LONG g_kbo_captain_last_attempted_season = 0;
KboCaptainSeed g_kbo_captain_seeds[KBO_CAPTAIN_SEED_MAX];
int g_kbo_captain_seed_count = 0;
volatile LONG g_kbo_captain_seed_lock = 0;
volatile LONG g_kbo_captain_seed_loaded = 0;
char g_kbo_captain_seed_loaded_key[MAX_PATH * 6] = {0};
