#ifndef KBOFIX_SRC_PLAYER_TEAM_HISTORY_INTERNAL_PLAYER_TEAM_SEASONS_INTERNAL_H_
#define KBOFIX_SRC_PLAYER_TEAM_HISTORY_INTERNAL_PLAYER_TEAM_SEASONS_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../player_team_seasons.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/sync/spin_lock.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_string.h"

#define KBO_PLAYER_TEAM_SEASON_SEED_MAX OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS

typedef struct KboPlayerTeamSeasonRow {
    char player_key[64];
    char team_code[16];
    int season_count;
} KboPlayerTeamSeasonRow;

extern KboPlayerTeamSeasonRow g_kbo_player_team_season_seed[KBO_PLAYER_TEAM_SEASON_SEED_MAX];
extern int g_kbo_player_team_season_seed_count;
extern volatile LONG g_kbo_player_team_season_seed_loaded;
extern volatile LONG g_kbo_player_team_season_seed_lock;

void kbo_player_team_seasons_lock(void);
void kbo_player_team_seasons_unlock(void);
KboCsvReader* kbo_player_team_seasons_open_seed(char* out_path, size_t out_path_size);
void kbo_player_team_seasons_copy_team_seed_code(uint32_t team_id, char* out, size_t out_size);
int kbo_player_team_seasons_copy_player_export_keys(uint8_t* player, char keys[][64], int max_count);
void kbo_player_team_seasons_ensure_seed_loaded(void);

#endif
