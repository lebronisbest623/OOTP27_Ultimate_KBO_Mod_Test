/* Save-scoped FA filing ledger captured from live OOTP transitions. */

#define KBO_FA_FILING_MAX 4096

typedef struct KboFaFilingRecord {
    uint32_t player_id;
    uint32_t filing_date;
    uint32_t season;
    uint32_t original_team_id;
    uint32_t league_id;
    uint32_t source_caller_rva;
    uint8_t notify;
    uint8_t contract_level;
    char player_name[96];
    char source[48];
} KboFaFilingRecord;

static volatile LONG g_kbo_fa_filing_lock = 0;
static volatile LONG g_kbo_fa_filing_cache_dirty = 1;
static KboFaFilingRecord* g_kbo_fa_filing_cache_rows = NULL;
static int g_kbo_fa_filing_cache_count = 0;
static char g_kbo_fa_filing_cache_path[MAX_PATH] = {0};

#define KBO_FA_FILING_NEGATIVE_CACHE_MAX 512
static uint32_t g_kbo_fa_filing_negative_cache[KBO_FA_FILING_NEGATIVE_CACHE_MAX];
static volatile LONG g_kbo_fa_filing_negative_cache_cursor = 0;
