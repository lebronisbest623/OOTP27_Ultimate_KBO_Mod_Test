/* Foreign waiver shared state and DTOs. Included from native/src/foreign_waiver_ai.inc. */

static uint32_t g_kbo_foreign_waiver_window_start_serial = 0;
static uint32_t g_kbo_foreign_waiver_window_end_serial = 0;
static uint32_t g_kbo_foreign_waiver_last_seen_yyyymmdd = 0;
static uint32_t g_kbo_foreign_waiver_last_close_event_end = 0;
static uint32_t g_kbo_foreign_waiver_start_event_date = 0;
static uint32_t g_kbo_foreign_waiver_close_event_end_date = 0;
static uint32_t g_kbo_foreign_waiver_last_result_announcement = 0;
static volatile LONG g_kbo_foreign_priority_pending_lock = 0;
static uint32_t g_kbo_foreign_priority_pending_yyyymmdd = 0;
static char g_kbo_foreign_priority_pending_title[96] = {0};
static char g_kbo_foreign_priority_pending_source[48] = {0};
static LONG g_kbo_foreign_waiver_rights_lock = 0;
static LONG g_kbo_foreign_waiver_decision_lock = 0;

#define KBO_FOREIGN_WAIVER_RETENTION_YEARS 5
#define KBO_FOREIGN_WAIVER_RIGHTS_MAX 1024
#define KBO_FOREIGN_WAIVER_AI_AUTO_TEAM_SLOT_MAX 512

typedef struct KboForeignWaiverRetention {
    uint32_t player_id;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t retained_on_yyyymmdd;
    uint32_t expires_on_yyyymmdd;
} KboForeignWaiverRetention;

typedef struct KboForeignWaiverAiTargetCandidate {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t current_team_id;
    int score;
    int forced;
} KboForeignWaiverAiTargetCandidate;

static KboForeignWaiverRetention g_kbo_foreign_waiver_rights[KBO_FOREIGN_WAIVER_RIGHTS_MAX] = {{0}};
static int g_kbo_foreign_waiver_rights_count = 0;


