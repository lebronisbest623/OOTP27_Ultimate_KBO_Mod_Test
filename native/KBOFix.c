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

/* ---- Native league events/news ---- */
#define OOTP27_CREATE_LEAGUE_EVENT_RVA 0x00B75060u
#define OOTP27_PISD_STRING_ASSIGN_RVA 0x01CC0D20u
#define OOTP27_LEAGUE_NEWS_REAL_ADD_RVA 0x004CB000u
#define OOTP27_NEWS_OBJECT_CTOR_RVA 0x006ED1B0u
#define OOTP27_NEWS_STRING_ENSURE_RVA 0x006EFC20u
#define OOTP27_CREATE_NEWS_ITEM_RVA 0x012B8BF0u
#define OOTP27_CREATE_MESSAGE_CORE_RVA 0x012F80C0u
#define OOTP27_SQL_EXEC_RVA 0x0122FDD0u
#define OOTP27_UI_OPERATOR_NEW_RVA 0x0278894Cu
#define OOTP27_PLAYER_TEAM_SIGNABILITY_RVA 0x0085C340u
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
#define OOTP27_PLAYER_OVERALL_VALUE_OFFSET         0xcd2u
#define OOTP27_PLAYER_TALENT_VALUE_OFFSET          0xcd4u
#define OOTP27_PLAYER_RATINGS_VALUE_OFFSET         0xcd6u
#define OOTP27_PLAYER_CAREER_VALUE_OFFSET          0xcd8u

/* ---- Team offsets ---- */
#define OOTP27_KBO_TEAM_VECTOR_OFFSET     0x90u
#define OOTP27_KBO_TEAM_COUNT_OFFSET      0x9cu
#define OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET  0x120u
#define OOTP27_KBO_TEAM_NATION_ID_OFFSET  0x128u
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

/* ---- Structs ---- */
typedef struct KboMilitaryActiveLoan {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t service_team_id;
    uint32_t service_league_id;
    uint32_t service_start_date_serial;
    int32_t  service_total_days;
    uintptr_t player_ptr;
} KboMilitaryActiveLoan;

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
static const char g_kbo_default_event_source[] = "custom_event_monitor";

/* ---- Forward declarations ---- */
__declspec(noinline) void ootp_kbo_military_service_entry_wrapper(
    uintptr_t player_ptr, uintptr_t original_func_ptr);
__declspec(noinline) void ootp_kbo_military_status_update_wrapper(
    uintptr_t player_ptr, uintptr_t original_func_ptr);
__declspec(noinline) int ootp_kbo_player_team_signability_wrapper(
    uintptr_t player_ptr, int32_t team_id, uint16_t year_hint, uintptr_t original_func_ptr);

static int memory_range_readable(const void* address, SIZE_T size);
static uintptr_t get_ootp_global_database(void);
static int kbo_player_pointer_plausible(uintptr_t player_ptr);
static int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset);
static int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size);
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
static void start_kbo_foreign_waiver_scanner_thread(void);
static void kbo_flush_pending_foreign_priority_events(const char* source);
static int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability);
static int start_kbo_custom_event_monitor(void);
static int kbo_schedule_foreign_priority_custom_events(const char* source);

/*
 * Single-translation-unit build: feature areas split into source fragments.
 */
#include "src/core.inc"
#include "src/build_verify.inc"
#include "src/runtime_memory.inc"
#include "src/team.inc"
#include "src/military_service_loan.inc"
#include "src/foreign_waiver_ai.inc"
#include "src/custom_events.inc"
#include "src/patch_helpers.inc"
#include "src/hook_stubs.inc"
#include "src/patch_installers.inc"
#include "src/hotkey_window.inc"
#include "src/entrypoint.inc"
