#ifndef NATIVE_SRC_FOREIGN_REPLACEMENT_SEED_FOREIGN_REPLACEMENT_SEED_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_REPLACEMENT_SEED_FOREIGN_REPLACEMENT_SEED_C_INTERNAL_H

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
#include "../parse/foreign_replacement_seed_parse.h"
#define KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX 128

extern KboForeignReplacementPlayerSeed g_kbo_foreign_replacement_player_seeds[KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX];
extern int g_kbo_foreign_replacement_player_seed_count;
extern char g_kbo_foreign_replacement_player_seed_loaded_path[MAX_PATH];
extern LONG g_kbo_foreign_replacement_player_seed_lock;
extern LONG64 g_kbo_foreign_replacement_player_seed_last_resolve_tick;

void kbo_lock_foreign_replacement_player_seeds(void);
void kbo_unlock_foreign_replacement_player_seeds(void);
int kbo_player_memory_contains_ascii_seed_key(uint8_t* player, const char* key);
uint32_t kbo_resolve_foreign_replacement_player_seed_key(const char* key, uint8_t* out_slot_type);
int kbo_add_foreign_replacement_player_seed_locked(const KboForeignReplacementPlayerSeed* seed);
int kbo_import_foreign_replacement_player_seed_file_locked(const char* path, const char* source);
int kbo_load_foreign_replacement_player_resolved_cache_locked(void);
int kbo_persist_foreign_replacement_player_resolved_cache_locked(void);
int kbo_resolve_foreign_replacement_player_seeds_locked(void);
void kbo_ensure_foreign_replacement_player_seeds_loaded(void);
int kbo_foreign_replacement_player_seed_matches_loaded(uint8_t* player, uint8_t* out_slot_type);

#endif
