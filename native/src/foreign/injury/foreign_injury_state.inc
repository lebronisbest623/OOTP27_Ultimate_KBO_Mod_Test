/* Foreign injury replacement slot lifecycle. Included from native/src/foreign_waiver_ai.inc. */

#define KBO_FOREIGN_INJURY_REPLACEMENT_MIN_DAYS 42
#define KBO_FOREIGN_INJURY_REPLACEMENT_MAX      256
#define KBO_FOREIGN_INJURY_STATUS_OPEN          1
#define KBO_FOREIGN_INJURY_STATUS_ACTIVE        2
#define KBO_FOREIGN_INJURY_STATUS_PENDING       3
#define KBO_FOREIGN_INJURY_STATUS_CLOSED        4

typedef struct KboForeignInjuryReplacement {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t injured_player_id;
    uint32_t replacement_player_id;
    uint32_t opened_on_yyyymmdd;
    uint32_t expected_end_yyyymmdd;
    uint8_t  slot_type;
    uint8_t  status;
    uint8_t  converted;
} KboForeignInjuryReplacement;

static KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX] = {{0}};
static int  g_kbo_foreign_injury_replacement_count = 0;
static LONG g_kbo_foreign_injury_replacement_lock = 0;
static char g_kbo_foreign_injury_replacement_loaded_path[MAX_PATH] = {0};

static int kbo_persist_foreign_injury_replacements_locked(void);
static int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed);

static int kbo_foreign_injury_replacement_enabled(void)
{
    return kbo_fix_enabled() && !read_kbo_localappdata_flag_file("disable_foreign_injury_replacement.txt");
}

