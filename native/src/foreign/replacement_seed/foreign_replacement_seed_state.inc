/* Foreign replacement-player seed resolution. Included from native/src/foreign_waiver_ai.inc. */

#define KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX 128
#define KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES 40

typedef struct KboForeignReplacementPlayerSeed {
    char key[KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES];
    uint32_t player_id;
    uint8_t slot_type;
} KboForeignReplacementPlayerSeed;

static KboForeignReplacementPlayerSeed g_kbo_foreign_replacement_player_seeds[KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX];
static int g_kbo_foreign_replacement_player_seed_count = 0;
static char g_kbo_foreign_replacement_player_seed_loaded_path[MAX_PATH] = {0};
static LONG g_kbo_foreign_replacement_player_seed_lock = 0;
static LONG64 g_kbo_foreign_replacement_player_seed_last_resolve_tick = 0;

static int kbo_persist_foreign_replacement_player_resolved_cache_locked(void);

