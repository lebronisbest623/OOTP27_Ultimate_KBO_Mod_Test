#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <WebView2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <intrin.h>

/* ---- Capacity constants ---- */
#define OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS  8192
#define OOTP27_KBO_MAX_ALLSTAR_TEAM_ROWS     8192

/* ---- Native league events/news ---- */
#define OOTP27_CREATE_LEAGUE_EVENT_RVA 0x00B75060u
#define OOTP27_PISD_STRING_ASSIGN_RVA 0x01CB8000u
#define OOTP27_LEAGUE_NEWS_REAL_ADD_RVA 0x004CB0D0u
#define OOTP27_NEWS_OBJECT_CTOR_RVA 0x006ECD50u
#define OOTP27_NEWS_STRING_ENSURE_RVA 0x006EF7C0u
#define OOTP27_CREATE_NEWS_ITEM_RVA 0x012B8BF0u
#define OOTP27_CREATE_MESSAGE_CORE_RVA 0x012F80C0u
#define OOTP27_SQL_EXEC_RVA 0x0122FDD0u
#define OOTP27_SQL_EXEC_DIRECT_RVA 0x012310A0u
#define OOTP27_UI_OPERATOR_NEW_RVA 0x0277E9DCu
#define OOTP27_GLOBAL_HANDLE_CALLBACK_RVA 0x00F40630u
#define OOTP27_PLAYER_PAGE_CTOR_RVA 0x01374630u
#define OOTP27_PLAYER_PROFILE_PAGE_CTOR_RVA 0x017BC980u
#define OOTP27_UI_OPEN_PAGE_RVA 0x005DF750u
#define OOTP27_UI_CURRENT_MODE_RVA 0x005DF630u
#define OOTP27_UI_CURRENT_PAGE_RVA 0x005DF5E0u
#define OOTP27_UI_CONTEXT_ROOT_RVA 0x031EEEA0u
#define OOTP27_UI_CONTEXT_ACTIVE_INDEX_RVA 0x031EEEC0u
#define OOTP27_UI_CONTEXT_FALLBACK_INDEX_RVA 0x031EEEAAu
#define OOTP27_PLAYER_TEAM_SIGNABILITY_RVA 0x0085C340u
#define OOTP27_PLAYER_SIGNABILITY_BLOCK_849320_RVA 0x00849320u
#define OOTP27_AI_FA_SIGNABILITY_RANDOM_FALLBACK_RVA 0x00AF1D83u
#define OOTP27_FA_SUBMIT_OFFER_PROBE_RVA 0x017A64B0u
#define OOTP27_FA_SIGNING_BRANCH_PATCH_OFFSET 0x27u
#define OOTP27_AI_FA_STATUS_CANDIDATE_INSERT_RVA 0x00AF220Bu
#define OOTP27_ACTIVE_FOREIGN_HITTER_COUNT_RVA 0x00A32B30u
#define OOTP27_ACTIVE_FOREIGN_PITCHER_COUNT_RVA 0x00A32BC0u
#define OOTP27_SECONDARY_FOREIGN_HITTER_COUNT_RVA 0x00A32C50u
#define OOTP27_SECONDARY_FOREIGN_PITCHER_COUNT_RVA 0x00A32CE0u
#define OOTP27_CALLUP_FOREIGN_HITTER_LIMIT_BRANCH_RVA 0x01079A85u
#define OOTP27_CALLUP_FOREIGN_PITCHER_LIMIT_BRANCH_RVA 0x01079B72u
#define OOTP27_CALLUP_FOREIGN_TOTAL_LIMIT_BRANCH_RVA 0x01079C22u
#define OOTP27_ALLSTAR_PREP_SINGLE_DIVISION_GATE_RVA 0x0065CC0Bu
#define OOTP27_ALLSTAR_ROSTER_SINGLE_DIVISION_GATE_RVA 0x0065DC09u
#define OOTP27_IMPORT_SCHEDULE_ALLSTAR_DAY_GATE_RVA 0x008E6BA5u
#define OOTP27_ALLSTAR_SETTINGS_ENABLE_VALUE_SITE_RVA 0x01544357u
#define OOTP27_ALLSTAR_SETTINGS_SINGLE_DIVISION_DISABLE_RVA 0x01544377u
#define OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_SITE_RVA 0x006D734Au
#define OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_DONE_RVA 0x006D74B8u
#define OOTP27_ALLSTAR_VOTING_BEGIN_PREP_SITE_RVA 0x00763D5Bu
#define OOTP27_ALLSTAR_VOTING_BEGIN_NO_GAME_RVA 0x00763F5Eu
#define OOTP27_MAKE_ALLSTAR_EVENTS_PREP_SITE_RVA 0x00667624u
#define OOTP27_ALLSTAR_TEAM_SETUP_SINGLE_DIVISION_BAIL_RVA 0x00667056u
#define OOTP27_UI_CHECKBOX_SET_BOOL_RVA 0x01E7A420u
#define OOTP27_VECTOR_PUSH_BACK_RVA 0x01CC3760u
#define OOTP27_MESSAGE_OBJECT_SIZE 0xA8u
#define OOTP27_MESSAGE_OBJECT_ID_OFFSET 0x8Cu
#define OOTP27_MESSAGE_SAVE_PATH_OFFSET 0x1A58u

/* ---- Player offsets ---- */
#define OOTP27_PLAYER_SCAN_BYTES                   0x1800u
#define OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET     0x48u
#define OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET        0x54u
#define OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET       0x58u
#define OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET        0x60u
#define OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET          0x68u
#define OOTP27_PLAYER_NATION_ID_OFFSET             0x6cu
#define OOTP27_KBO_KOREA_NATION_ID                 177u
#define OOTP27_PLAYER_AGE_OFFSET                   0x7cu
#define OOTP27_PLAYER_ID_OFFSET                    0xb4u
#define OOTP27_PLAYER_GIVEN_NAME_ID_OFFSET         0x20u
#define OOTP27_PLAYER_FAMILY_NAME_ID_OFFSET        0x24u
#define OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET       0xd70u
#define OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET      0x842u
#define OOTP27_PLAYER_LOAN_CLEARED_MARKER_OFFSET   0x843u
#define OOTP27_PLAYER_DFA_FLAG_OFFSET              0x85bu
#define OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET       0xf69u
#define OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET       0x83bu
#define OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET 0x83cu
#define OOTP27_PLAYER_INJURY_ACTIVE_OFFSET         0x874u
#define OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET      0x876u
#define OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET       0x895u
#define OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET       0x896u
#define OOTP27_PLAYER_POSITION_GROUP_OFFSET        0xc78u
#define OOTP27_PLAYER_OVERALL_VALUE_OFFSET         0xcd2u
#define OOTP27_PLAYER_TALENT_VALUE_OFFSET          0xcd4u
#define OOTP27_PLAYER_RATINGS_VALUE_OFFSET         0xcd6u
#define OOTP27_PLAYER_CAREER_VALUE_OFFSET          0xcd8u

/* ---- Team offsets ---- */
#define OOTP27_KBO_TEAM_VECTOR_OFFSET     0x90u
#define OOTP27_KBO_TEAM_COUNT_OFFSET      0x9cu
#define OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET  0x120u
#define OOTP27_KBO_TEAM_NATION_ID_OFFSET  0x128u
#define OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET 0x130u
#define OOTP27_KBO_TEAM_DELETED_OFFSET    0xc58u
#define OOTP27_KBO_TEAM_ID_OFFSET         0x4450u
#define OOTP27_KBO_TEAM_READABLE_BYTES    0x4460u
#define OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET 0x8u
#define OOTP27_KBO_TEAM_CITY_STRING_OFFSET   0x10u

/* ---- Team roster array offsets ---- */
#define OOTP27_TEAM_PLAYER_IDS_2760_OFFSET         0x2760u
#define OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET         0x2a80u
#define OOTP27_TEAM_PLAYER_IDS_2DA0_OFFSET         0x2da0u
#define OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET   0x30c0u
#define OOTP27_TEAM_PLAYER_IDS_33E0_OFFSET         0x33e0u
#define OOTP27_TEAM_PLAYER_IDS_3700_OFFSET         0x3700u
#define OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT          200u

/* ---- Global DB offsets ---- */
#define OOTP27_GLOBAL_CURRENT_DATE_OFFSET          0x2d8u
#define OOTP27_GLOBAL_SQL_DATABASE_OFFSET          0x2d78u
#define OOTP27_GLOBAL_SQL_ENABLED_OFFSET           0x2df0u
#define OOTP27_CURRENT_DATE_YEAR_OFFSET            0x1b8u
#define OOTP27_CURRENT_DATE_MONTH_OFFSET           0x1bbu
#define OOTP27_CURRENT_DATE_DAY_OFFSET             0x1bau

/* ---- League custom events ---- */
#define OOTP27_KBO_MAIN_LEAGUE_ID                  100u
#define OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET   0x250u
#define OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET    0x25cu
#define OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET       0x08u
#define OOTP27_LEAGUE_EVENT_YEAR_OFFSET            0x18u
#define OOTP27_LEAGUE_EVENT_DAY_OFFSET             0x1au
#define OOTP27_LEAGUE_EVENT_MONTH_OFFSET           0x1bu
#define OOTP27_LEAGUE_EVENT_TYPE_OFFSET            0x20u
#define OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET      0x22u
#define OOTP27_LEAGUE_EVENT_DELETED_OFFSET         0x23u
#define OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET     0x28u
#define OOTP27_EVENT_TYPE_CUSTOM_EVENT             0u
#define OOTP27_EVENT_TYPE_DRAFT_POOL_ANNOUNCEMENT  3u

/* ---- League offsets ---- */
#define OOTP27_KBO_LEAGUE_ID_OFFSET    0x4cc0u
#define OOTP27_KBO_LEAGUE_YEAR_OFFSET  0x44ecu

/* ---- Military service ---- */
#define KBO_MILITARY_SERVICE_DAYS 545

/* ---- Asian Games ---- */
#define KBO_ASIAN_GAMES_ROSTER_SIZE 24
#define KBO_ASIAN_GAMES_PITCHER_TARGET 11
#define KBO_ASIAN_GAMES_CATCHER_TARGET 2
#define KBO_ASIAN_GAMES_INFIELDER_TARGET 6
#define KBO_ASIAN_GAMES_OUTFIELDER_TARGET 5
#define KBO_ASIAN_GAMES_MAX_WILDCARDS 3
#define KBO_ASIAN_GAMES_MAX_CANDIDATES 4096
#define KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES 8
#define KBO_ASIAN_GAMES_MAX_ORGS 32
#define KBO_ASIAN_GAMES_TEAM_MIN_PLAYERS 1
#define KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS 3

/* ---- Structs ---- */
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

typedef struct KboAsianGamesRosterEntry {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t departure_date;
    uint32_t return_date;
    uint16_t age;
    uint8_t role;
    uint8_t wildcard;
    uint8_t old_restricted;
    uint8_t old_secondary_restricted;
    uint8_t old_injury_active;
    int16_t old_injury_days_left;
    uint8_t departed;
    uint8_t returned;
    uint8_t exempted;
    int32_t score;
    uintptr_t player_ptr;
} KboAsianGamesRosterEntry;

typedef struct KboAsianGamesCandidate {
    KboAsianGamesRosterEntry entry;
    uint32_t org_team_id;
    uint8_t selected;
} KboAsianGamesCandidate;

typedef struct KboAllstarTeamRow {
    uint16_t year;
    char team_id[16];
    char team_name[96];
    char current_city[64];
    uint8_t side;
} KboAllstarTeamRow;

typedef void* (__fastcall *OotpCreateLeagueEventFn)(
    void* event_manager,
    void* date,
    uint32_t event_type,
    uint32_t league_id,
    const char* title,
    uint16_t aux_id);

typedef uint8_t (__fastcall *OotpCreateNewsItemFn)(
    uint32_t news_type,
    uint32_t team_id,
    void* date,
    const char* text,
    uint8_t immediate);

typedef void (__fastcall *OotpCreateMessageCoreFn)(
    void* context,
    uint32_t internal_message_type,
    uint32_t team_id,
    void* date,
    const char* text);

typedef int (__fastcall *OotpSqlExecFn)(void* database, const char* sql);

typedef uint32_t (__fastcall *OotpLeagueNewsRealAddFn)(
    void* manager,
    void* news_object,
    uint8_t flag);

typedef void* (__fastcall *OotpNewsObjectCtorFn)(void* news_object);
typedef void (__fastcall *OotpNewsStringEnsureFn)(void* news_object);
typedef void* (__fastcall *OotpOperatorNewFn)(size_t size);
typedef void* (__fastcall *OotpCoreStringAssignFn)(void* string_object, const char* text);

/* ---- Globals ---- */
static KboMilitaryActiveLoan g_active_military_loans[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS];
static LONG g_active_military_loan_count    = 0;
static LONG g_military_service_entry_log_count = 0;
static LONG g_military_loan_return_log_count   = 0;
static LONG g_military_days_tick_started       = 0;
static LONG g_military_days_tick_log_count     = 0;
static KboAsianGamesRosterEntry g_kbo_asian_games_roster[KBO_ASIAN_GAMES_ROSTER_SIZE];
static LONG g_kbo_asian_games_roster_count = 0;
static uint32_t g_kbo_asian_games_roster_year = 0;
static char g_kbo_asian_games_roster_save_path[MAX_PATH] = {0};
static KboAllstarTeamRow g_allstar_team_rows[OOTP27_KBO_MAX_ALLSTAR_TEAM_ROWS];
static int g_allstar_team_row_count = 0;
static LONG g_allstar_team_rules_loaded = 0;
static LONG g_allstar_candidate_seed_log_count = 0;
static LONG g_allstar_voting_prepare_log_count = 0;
static LONG g_allstar_team_name_log_count = 0;
static const char g_kbo_default_event_source[] = "custom_event_monitor";

/* ---- Forward declarations ---- */
__declspec(noinline) void ootp_kbo_military_service_entry_wrapper(
    uintptr_t player_ptr, uintptr_t original_func_ptr);
__declspec(noinline) void ootp_kbo_military_status_update_wrapper(
    uintptr_t player_ptr, uintptr_t original_func_ptr);
__declspec(noinline) int ootp_kbo_player_team_signability_wrapper(
    uintptr_t player_ptr, int32_t team_id, uint16_t year_hint, uintptr_t original_func_ptr);
__declspec(noinline) uint8_t ootp_kbo_player_offer_eligibility_wrapper(
    uintptr_t player_ptr, int32_t team_id, int32_t flag, uintptr_t original_func_ptr);
__declspec(noinline) void ootp_kbo_fa_submit_offer_probe_wrapper(
    uintptr_t screen_ptr, uintptr_t original_func_ptr);
__declspec(noinline) void ootp_kbo_fa_signing_branch_wrapper(
    uintptr_t player_ptr, uintptr_t team_ptr);
__declspec(noinline) int32_t ootp_kbo_ai_fa_status_candidate_insert_wrapper(
    uintptr_t frame_ptr, uintptr_t player_ptr, int32_t insert_index, uintptr_t candidate_array);
__declspec(noinline) int32_t ootp_kbo_active_foreign_hitter_count_wrapper(
    uintptr_t team_ptr, uintptr_t original_func_ptr);
__declspec(noinline) int32_t ootp_kbo_active_foreign_pitcher_count_wrapper(
    uintptr_t team_ptr, uintptr_t original_func_ptr);
__declspec(noinline) uint8_t ootp_kbo_callup_foreign_hitter_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit);
__declspec(noinline) uint8_t ootp_kbo_callup_foreign_pitcher_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit);
__declspec(noinline) uint8_t ootp_kbo_callup_foreign_total_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit);
static int memory_range_readable(const void* address, SIZE_T size);
static uintptr_t get_ootp_global_database(void);
static int kbo_player_pointer_plausible(uintptr_t player_ptr);
static int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset);
static int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size);
static int kbo_get_current_save_path(char* out, size_t out_size);
static void kbo_copy_player_display_name(uint8_t* player, char* out, size_t out_size);
static uint8_t* find_kbo_team_by_csv_id_any_league(const char* team_id, int allow_deleted);
static uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted);
static int create_kbo_native_live_news_with_body(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body);
static int create_kbo_native_live_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title);
static int insert_kbo_player_history_sql(
    uint32_t player_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const char* text,
    const char* source);
static int insert_kbo_roster_transaction_sql(
    uint32_t league_id,
    uint32_t team_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t transaction_type,
    const char* league_text,
    const char* team_text,
    const char* source);
static void start_kbo_foreign_waiver_scanner_thread(void);
static void start_kbo_foreign_injury_replacement_thread(void);
static void start_kbo_fa_requalification_thread(void);
static void kbo_flush_pending_foreign_priority_events(const char* source);
static int kbo_get_current_yyyymmdd(uint32_t* out_date);
static int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability,
    uintptr_t caller_rva);
static int start_kbo_custom_event_monitor(void);
static int kbo_schedule_foreign_priority_custom_events(const char* source);
static void kbo_hub_copy_player_display_name(uint8_t* player, char* out, size_t out_size);
__declspec(noinline) int ootp_kbo_seed_single_division_allstar_candidate_teams(
    uintptr_t league_ptr,
    void* left_team_vector,
    void* right_team_vector,
    uintptr_t vector_push_back_ptr);
__declspec(noinline) void ootp_kbo_enable_allstar_setting(uintptr_t league_ptr);
__declspec(noinline) void ootp_kbo_prepare_allstar_events(uintptr_t league_ptr);
__declspec(noinline) void ootp_kbo_prepare_allstar_voting_begin(uintptr_t league_ptr, uintptr_t allstar_team_setup_ptr);

/*
 * Single-translation-unit build: feature areas split into source fragments.
 */
#include "src/core.inc"
#include "src/build_verify.inc"
#include "src/runtime_memory.inc"
#include "src/team.inc"
#include "src/fa_requalification.inc"
#include "src/military_service_loan.inc"
#include "src/foreign_waiver_ai.inc"
#include "src/custom_events.inc"
#include "src/allstar.inc"
#include "src/patch_helpers.inc"
#include "src/hook_stubs.inc"
#include "src/patch_installers.inc"
#include "src/hotkey_window.inc"
#include "src/entrypoint.inc"
