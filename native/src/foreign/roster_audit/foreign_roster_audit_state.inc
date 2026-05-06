/* Foreign roster audit and snapshots. Included from native/src/foreign_waiver_ai.inc. */

#define KBO_FOREIGN_ROSTER_AUDIT_MAX 8192

typedef struct KboForeignRosterAuditState {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t current_league_id;
    uint32_t loan_team_id;
    uint8_t restricted;
    uint8_t secondary_restricted;
    uint8_t dfa;
    uint8_t loan_active;
    uint8_t injury_active;
    int score;
    uint32_t seen_generation;
} KboForeignRosterAuditState;

static KboForeignRosterAuditState g_kbo_foreign_roster_audit[KBO_FOREIGN_ROSTER_AUDIT_MAX] = {{0}};
static int g_kbo_foreign_roster_audit_count = 0;
static uint32_t g_kbo_foreign_roster_audit_generation = 0u;
static char g_kbo_foreign_roster_audit_save_path[MAX_PATH] = {0};

