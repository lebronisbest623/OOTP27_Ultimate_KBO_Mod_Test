/* Read-only KBO FA-market classification diagnostics. */

#define KBO_FA_MARKET_CLASSIFICATION_MAX 1024
#define KBO_FA_MARKET_SEED_MAX 1024
#define KBO_FA_MARKET_INDEPENDENT_LEAGUE_ID 200u
#define KBO_FA_MARKET_HISTORY_TEXT_MAX 192

typedef struct KboFaMarketSeedCase {
    uint32_t player_id;
    char case_label[48];
    char grade[12];
    char note[128];
} KboFaMarketSeedCase;

typedef struct KboFaMarketClassification {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t current_league_id;
    uint32_t draft_league_id;
    uint32_t rights_team_id;
    uint16_t age;
    uint8_t contract_level;
    uint8_t dfa;
    uint8_t foreign_player;
    uint8_t retired_flag;
    uint8_t draft_class;
    uint8_t draft_subtype;
    uint8_t draft_eligible;
    uint8_t draft_extra;
    uint8_t generation_flags;
    uint8_t generation_context;
    uint8_t generation_grade;
    uint8_t generation_special;
    int32_t fa_demand;
    int32_t fa_grade_salary;
    uint32_t fa_grade_overall_rank;
    uint32_t fa_grade_team_rank;
    uint32_t fa_grade_snapshot_team_id;
    uint32_t fa_grade_snapshot_date;
    uint32_t fa_grade_opening_day;
    uint8_t fa_grade_auto;
    uint8_t fa_grade_team_changed_review;
    char player_name[96];
    char case_label[48];
    char grade[12];
    char fa_grade_flag[48];
    char reason[224];
} KboFaMarketClassification;

typedef struct KboFaMarketScanSummary {
    uint32_t league_id;
    uint32_t today_yyyymmdd;
    uint32_t current_year;
    int scanned;
    int candidates;
    int rows;
    int truncated;
    int seed_count;
    int requalification_count;
    int salary_snapshot_count;
    int csv_written;
    char csv_path[MAX_PATH];
    char seed_path[MAX_PATH];
    char salary_snapshot_path[MAX_PATH];
} KboFaMarketScanSummary;

typedef struct KboFaMarketHistoryCase {
    uint32_t player_id;
    int found;
    int became_free_agent;
    int undrafted_free_agent;
    int released;
    char history_date[16];
    char history_text[KBO_FA_MARKET_HISTORY_TEXT_MAX];
} KboFaMarketHistoryCase;

typedef struct KboFaMarketSqlite3 KboFaMarketSqlite3;
typedef struct KboFaMarketSqlite3Stmt KboFaMarketSqlite3Stmt;
typedef int (__cdecl *KboSqlite3OpenV2Fn)(const char*, KboFaMarketSqlite3**, int, const char*);
typedef int (__cdecl *KboSqlite3CloseFn)(KboFaMarketSqlite3*);
typedef int (__cdecl *KboSqlite3PrepareV2Fn)(KboFaMarketSqlite3*, const char*, int, KboFaMarketSqlite3Stmt**, const char**);
typedef int (__cdecl *KboSqlite3StepFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3FinalizeFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3BindIntFn)(KboFaMarketSqlite3Stmt*, int, int);
typedef int (__cdecl *KboSqlite3ResetFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3ClearBindingsFn)(KboFaMarketSqlite3Stmt*);
typedef const unsigned char* (__cdecl *KboSqlite3ColumnTextFn)(KboFaMarketSqlite3Stmt*, int);
typedef const char* (__cdecl *KboSqlite3ErrmsgFn)(KboFaMarketSqlite3*);

typedef struct KboFaMarketSqliteApi {
    HMODULE module;
    int attempted;
    int available;
    KboSqlite3OpenV2Fn open_v2;
    KboSqlite3CloseFn close;
    KboSqlite3PrepareV2Fn prepare_v2;
    KboSqlite3StepFn step;
    KboSqlite3FinalizeFn finalize;
    KboSqlite3BindIntFn bind_int;
    KboSqlite3ResetFn reset;
    KboSqlite3ClearBindingsFn clear_bindings;
    KboSqlite3ColumnTextFn column_text;
    KboSqlite3ErrmsgFn errmsg;
} KboFaMarketSqliteApi;

typedef struct KboFaMarketFileSignature {
    int exists;
    DWORD size_high;
    DWORD size_low;
    FILETIME last_write_time;
} KboFaMarketFileSignature;

static KboFaMarketSqliteApi g_kbo_fa_market_sqlite_api = {0};
static KboFaMarketHistoryCase g_kbo_fa_market_history_cache[KBO_FA_MARKET_CLASSIFICATION_MAX];
static int g_kbo_fa_market_history_cache_count = 0;
static int g_kbo_fa_market_history_cache_valid = 0;
static char g_kbo_fa_market_history_cache_save_path[MAX_PATH] = {0};
static KboFaMarketFileSignature g_kbo_fa_market_history_cache_db_sig = {0};
static KboFaMarketFileSignature g_kbo_fa_market_history_cache_wal_sig = {0};
static KboFaMarketFileSignature g_kbo_fa_market_history_cache_shm_sig = {0};

#define KBO_SQLITE_OK 0
#define KBO_SQLITE_ROW 100
#define KBO_SQLITE_DONE 101
#define KBO_SQLITE_OPEN_READONLY 0x00000001
