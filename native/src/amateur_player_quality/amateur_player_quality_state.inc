/* Amateur player assignment rerouting from team reputation seed. */

#define KBO_COLLEGE_LEAGUE_ID 201u
#define KBO_HIGH_SCHOOL_LEAGUE_ID 203u
#define KBO_AMATEUR_REPUTATION_SEED_MAX 384
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX 16384
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX 32768
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK (KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX - 1)
#define KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX 128
#define KBO_AMATEUR_REPUTATION_UPDATE_MIN_GAMES 10u
#define KBO_HIGH_SCHOOL_ASSIGNMENT_SOURCE_MIN_PLAYERS 18
#define KBO_COLLEGE_ASSIGNMENT_SOURCE_MIN_PLAYERS 24
#define KBO_AMATEUR_ASSIGNMENT_SOURCE_MIN_HITTERS 9
#define KBO_HIGH_SCHOOL_ASSIGNMENT_TARGET_MAX_PLAYERS 36
#define KBO_COLLEGE_ASSIGNMENT_TARGET_MAX_PLAYERS 40
#define KBO_AMATEUR_TEAM_REGULAR_WINS_OFFSET   0x400cu
#define KBO_AMATEUR_TEAM_REGULAR_LOSSES_OFFSET 0x4018u
#define KBO_AMATEUR_TEAM_REGULAR_TIES_OFFSET   0x401cu

typedef struct KboAmateurReputationSeed {
    uint32_t league_id;
    uint32_t team_id;
    char team_abbr[32];
    char team_name[96];
    char nick_name[64];
    uint8_t reputation;
} KboAmateurReputationSeed;

typedef struct KboAmateurReputationUpdateRow {
    uint32_t league_id;
    uint32_t team_id;
    uint8_t old_reputation;
    uint8_t new_reputation;
    uint16_t wins;
    uint16_t losses;
    uint16_t ties;
    int32_t score;
} KboAmateurReputationUpdateRow;

typedef struct KboAmateurAssignmentProcessed {
    uint32_t player_id;
    uint32_t team_id;
} KboAmateurAssignmentProcessed;

#define KBO_AMATEUR_ASSIGNMENT_TEAM_MAX 512

typedef struct KboAmateurAssignmentCandidate {
    uint8_t* team;
    uint32_t team_id;
    uint8_t reputation;
    int32_t player_count;
    int32_t hitter_count;
} KboAmateurAssignmentCandidate;

typedef struct KboAmateurResolvedTeamReputation {
    uint8_t* team;
    uint32_t league_id;
    uint8_t reputation;
} KboAmateurResolvedTeamReputation;

static KboAmateurReputationSeed g_kbo_amateur_reputation_seeds[KBO_AMATEUR_REPUTATION_SEED_MAX];
static int g_kbo_amateur_reputation_seed_count = 0;
static LONG g_kbo_amateur_reputation_seed_lock = 0;
static LONG g_kbo_amateur_reputation_seed_loaded = 0;
static char g_kbo_amateur_reputation_seed_loaded_path[MAX_PATH * 3] = {0};
static KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed[KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX];
static KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed_hash[KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX];
static LONG g_kbo_amateur_assignment_processed_count = 0;
static LONG g_kbo_amateur_assignment_processed_hash_count = 0;
static KboAmateurAssignmentCandidate g_kbo_amateur_assignment_high_school_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
static KboAmateurAssignmentCandidate g_kbo_amateur_assignment_college_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
static KboAmateurAssignmentProcessed g_kbo_amateur_assignment_rejected_targets[KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX];
static int g_kbo_amateur_assignment_high_school_count = -1;
static int g_kbo_amateur_assignment_college_count = -1;
static LONG g_kbo_amateur_assignment_rejected_target_count = 0;
static LONG g_kbo_amateur_assignment_candidate_lock = 0;
static KboAmateurResolvedTeamReputation g_kbo_amateur_resolved_team_reputations[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
static int g_kbo_amateur_resolved_team_reputation_count = 0;
static uint32_t g_kbo_amateur_reputation_last_update_high_school_year = 0u;
static uint32_t g_kbo_amateur_reputation_last_update_college_year = 0u;

static void kbo_lock_amateur_reputation_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_reputation_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_amateur_reputation_seeds(void)
{
    InterlockedExchange(&g_kbo_amateur_reputation_seed_lock, 0);
}

static void kbo_lock_amateur_assignment_candidates(void)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_assignment_candidate_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_amateur_assignment_candidates(void)
{
    InterlockedExchange(&g_kbo_amateur_assignment_candidate_lock, 0);
}

static int* kbo_amateur_assignment_count_ptr_for_league(uint32_t league_id)
{
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return &g_kbo_amateur_assignment_high_school_count;
    }
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return &g_kbo_amateur_assignment_college_count;
    }
    return NULL;
}

static KboAmateurAssignmentCandidate* kbo_amateur_assignment_cache_for_league(uint32_t league_id)
{
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return g_kbo_amateur_assignment_high_school_candidates;
    }
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return g_kbo_amateur_assignment_college_candidates;
    }
    return NULL;
}

static void kbo_invalidate_amateur_assignment_candidate_cache(void)
{
    kbo_lock_amateur_assignment_candidates();
    g_kbo_amateur_assignment_high_school_count = -1;
    g_kbo_amateur_assignment_college_count = -1;
    g_kbo_amateur_resolved_team_reputation_count = 0;
    InterlockedExchange(&g_kbo_amateur_assignment_rejected_target_count, 0);
    memset(g_kbo_amateur_assignment_high_school_candidates, 0, sizeof(g_kbo_amateur_assignment_high_school_candidates));
    memset(g_kbo_amateur_assignment_college_candidates, 0, sizeof(g_kbo_amateur_assignment_college_candidates));
    memset(g_kbo_amateur_resolved_team_reputations, 0, sizeof(g_kbo_amateur_resolved_team_reputations));
    memset(g_kbo_amateur_assignment_rejected_targets, 0, sizeof(g_kbo_amateur_assignment_rejected_targets));
    kbo_unlock_amateur_assignment_candidates();
}
