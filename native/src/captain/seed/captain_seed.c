#include "../internal/captain_selection_internal.h"
#include "../../core/csv/core_csv.h"
#include "../../core/sync/spin_lock.h"

static int kbo_captain_seed_source_rank(const char* source)
{
    return source != NULL && _stricmp(source, "save_seed") == 0 ? 2 : 1;
}

typedef struct KboCaptainDisplayCache {
    uint32_t season;
    uint32_t league_id;
    uint32_t team_id;
    uint32_t player_id;
    int found;
    char loaded_key[MAX_PATH * 6];
    char player_name[128];
    char source[24];
} KboCaptainDisplayCache;

static volatile LONG g_kbo_captain_display_cache_lock = 0;
static KboCaptainDisplayCache g_kbo_captain_display_cache;

void kbo_lock_captain_seeds(void)
{
    kbo_spin_lock(&g_kbo_captain_seed_lock);
}

void kbo_unlock_captain_seeds(void)
{
    kbo_spin_unlock(&g_kbo_captain_seed_lock);
}

static void kbo_lock_captain_display_cache(void)
{
    kbo_spin_lock(&g_kbo_captain_display_cache_lock);
}

static void kbo_unlock_captain_display_cache(void)
{
    kbo_spin_unlock(&g_kbo_captain_display_cache_lock);
}

#include "captain_seed_load.inc"

#include "captain_seed_match.inc"

#include "captain_seed_display.inc"
