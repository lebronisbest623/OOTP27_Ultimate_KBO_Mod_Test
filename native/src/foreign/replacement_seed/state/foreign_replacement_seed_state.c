#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../paths/foreign_replacement_seed_paths.h"
#include "../parse/foreign_replacement_seed_parse.h"
#include "../internal/foreign_replacement_seed_internal.h"

#include "../parse/foreign_replacement_seed_parse.h"

#define KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX 128

KboForeignReplacementPlayerSeed g_kbo_foreign_replacement_player_seeds[KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX];
int g_kbo_foreign_replacement_player_seed_count = 0;
char g_kbo_foreign_replacement_player_seed_loaded_path[MAX_PATH] = {0};
KboLock g_kbo_foreign_replacement_player_seed_lock = KBO_LOCK_INIT;
LONG64 g_kbo_foreign_replacement_player_seed_last_resolve_tick = 0;

int kbo_persist_foreign_replacement_player_resolved_cache_locked(void);

/* Foreign replacement-player seed lock helpers. Included from native/KBOFix.c. */

/* Foreign replacement-player seed memory-key resolution. Included from native/KBOFix.c. */

/* Foreign replacement-player seed import table. Included from native/KBOFix.c. */

/* Foreign replacement-player resolved cache IO. Included from native/KBOFix.c. */

/* Foreign replacement-player seed lazy-load and resolve orchestration. Included from native/KBOFix.c. */

/* Foreign replacement-player seed match API. Included from native/KBOFix.c. */

