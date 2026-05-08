static uint32_t  g_kbo_league_ptr_cache_id  = 0;
static uintptr_t g_kbo_league_ptr_cache_ptr = 0;
#define KBO_LEAGUE_PTR_MISS_CACHE_MAX 32
static uint32_t  g_kbo_league_ptr_miss_cache_ids[KBO_LEAGUE_PTR_MISS_CACHE_MAX] = {0};
static ULONGLONG g_kbo_league_ptr_miss_cache_until_ms[KBO_LEAGUE_PTR_MISS_CACHE_MAX] = {0};

#define KBO_NAMED_LEAGUE_SCAN_MIN_SCORE 90
#define KBO_NAMED_LEAGUE_SCAN_EARLY_SCORE 115
#define KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN (OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
#define KBO_LEAGUE_DISPLAY_CACHE_MAX 64

typedef struct KboHubLeagueDisplayCacheEntry {
    uint32_t league_id;
    uintptr_t league_ptr;
    uint32_t year;
    int score;
    char name[96];
    char logo_file[128];
} KboHubLeagueDisplayCacheEntry;

static uintptr_t g_kbo_league_display_cache_global = 0;
static uintptr_t g_kbo_league_display_cache_prewarmed_global = 0;
static KboHubLeagueDisplayCacheEntry g_kbo_league_display_cache[KBO_LEAGUE_DISPLAY_CACHE_MAX];

