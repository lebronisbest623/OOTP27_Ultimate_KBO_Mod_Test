/* Military service seed, draft, active-loan, and return-history state. Included from native/src/military_service_loan.inc. */

#define KBO_MILITARY_SERVICE_DAYS 545

typedef struct KboMilitaryActiveLoan {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t service_team_id;
    uint32_t service_league_id;
    uint32_t service_start_date_serial;
    uint32_t service_return_date_serial;
    int32_t  service_total_days;
    uintptr_t player_ptr;
} KboMilitaryActiveLoan;

#define KBO_MILITARY_SERVICE_SEED_MAX        512
#define KBO_MILITARY_SERVICE_SEED_KEY_BYTES  40
#define KBO_MILITARY_SERVICE_TEAM_BYTES      12

typedef struct KboMilitaryServiceSeed {
    char key[KBO_MILITARY_SERVICE_SEED_KEY_BYTES];
    uint32_t player_id;
    char service_team_code[KBO_MILITARY_SERVICE_TEAM_BYTES];
    char original_team_code[KBO_MILITARY_SERVICE_TEAM_BYTES];
    uint32_t service_start_yyyymmdd;
    uint32_t service_return_yyyymmdd;
    int32_t service_total_days;
} KboMilitaryServiceSeed;

typedef struct KboMilitaryDraftCandidate {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint16_t entry_year;
    uint8_t selected;
    uintptr_t player_ptr;
} KboMilitaryDraftCandidate;

typedef struct KboMilitarySelectionNewsEntry {
    uint32_t player_id;
    uint32_t original_team_id;
    int32_t score;
    uintptr_t player_ptr;
} KboMilitarySelectionNewsEntry;

typedef struct KboMilitaryReturnHistoryKey {
    uint32_t player_id;
    uint32_t history_yyyymmdd;
} KboMilitaryReturnHistoryKey;

static KboMilitaryActiveLoan g_active_military_loans[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS];
static LONG g_active_military_loan_count = 0;
static LONG g_military_service_entry_log_count = 0;
static LONG g_military_loan_return_log_count = 0;
static LONG g_military_days_tick_started = 0;
static LONG g_military_days_tick_log_count = 0;
static LONG g_military_seed_bootstrap_started = 0;
static LONG g_military_seed_expired_skip_log_count = 0;
static KboMilitaryServiceSeed g_kbo_military_service_seeds[KBO_MILITARY_SERVICE_SEED_MAX];
static int g_kbo_military_service_seed_count = 0;
static LONG g_kbo_military_service_seed_lock = 0;
static LONG g_kbo_military_service_seed_loaded = 0;
static ULONGLONG g_kbo_military_service_seed_last_resolve_tick = 0;
static char g_kbo_military_service_seed_loaded_key[MAX_PATH * 3];
static KboMilitaryDraftCandidate g_kbo_military_draft_candidates[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS];
static LONG g_kbo_military_draft_candidate_count = 0;
static KboMilitaryReturnHistoryKey g_kbo_military_return_history_keys[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS];
static LONG g_kbo_military_return_history_key_count = 0;

