/* Foreign replacement-player seed resolution. Included from native/src/foreign_waiver_ai.inc. */

#include "foreign_replacement_seed_parse.h"

#define KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX 128

static KboForeignReplacementPlayerSeed g_kbo_foreign_replacement_player_seeds[KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX];
static int g_kbo_foreign_replacement_player_seed_count = 0;
static char g_kbo_foreign_replacement_player_seed_loaded_path[MAX_PATH] = {0};
static LONG g_kbo_foreign_replacement_player_seed_lock = 0;
static LONG64 g_kbo_foreign_replacement_player_seed_last_resolve_tick = 0;

static int kbo_persist_foreign_replacement_player_resolved_cache_locked(void);

