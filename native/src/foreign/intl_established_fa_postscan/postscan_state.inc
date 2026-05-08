/* Post-registration scan for international established free agents. Included from native/src/foreign_waiver_ai.inc. */

#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_DELAY_MS 4500ull
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS 2500ull
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES 8
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_DETAIL_LOGS 160
#define KBO_INTL_ESTABLISHED_FA_ASIAN_STARTER_SCORE_CAP 72000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_BULLPEN_SCORE_CAP 70000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_UNKNOWN_PITCHER_SCORE_CAP 71000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_HITTER_SCORE_CAP 72000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_CATCHER_SCORE_CAP 65000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_STARTER_SCORE_CAP 115000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_BULLPEN_SCORE_CAP 95000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_UNKNOWN_PITCHER_SCORE_CAP 105000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_HITTER_SCORE_CAP 110000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_CATCHER_SCORE_CAP 80000

typedef struct KboIntlEstablishedFaPostscanState {
    LONG pending;
    LONG batch_id;
    int32_t before_count;
    uint32_t before_max_player_id;
    int32_t original_count;
    int32_t expected_count;
    int multiplier;
    uint32_t primary_league_id;
    uint32_t fallback_league_id;
    uint32_t scheduled_date;
    ULONGLONG due_tick;
    int attempts;
} KboIntlEstablishedFaPostscanState;

static KboIntlEstablishedFaPostscanState g_kbo_intl_established_fa_postscan = {0};
static volatile LONG g_kbo_intl_established_fa_postscan_worker_started = 0;

