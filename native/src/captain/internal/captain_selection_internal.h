#ifndef KBOFIX_SRC_CAPTAIN_INTERNAL_CAPTAIN_SELECTION_INTERNAL_H_
#define KBOFIX_SRC_CAPTAIN_INTERNAL_CAPTAIN_SELECTION_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/teams/core_team_collect.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../../team/names/team_string.h"
#include "../api/captain_selection.h"

#define KBO_CAPTAIN_MAX_TEAMS 64

typedef struct KboCaptainSelectionRow {
    uint32_t date;
    uint32_t season;
    uint32_t league_id;
    uint32_t team_id;
    char team_name[128];
    uint32_t player_id;
    char player_name[128];
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t current_league_id;
    uint16_t age;
    int32_t salary;
    int32_t value_score;
    int32_t score;
    int16_t overall_value;
    int16_t talent_value;
    int16_t ratings_value;
    int16_t career_value;
    uint8_t domestic;
    uint8_t dfa;
    uint8_t restricted;
    uint8_t injured;
    char reason[96];
} KboCaptainSelectionRow;

extern volatile LONG g_kbo_captain_preseason_thread_started;
extern volatile LONG g_kbo_captain_last_attempted_season;

int kbo_captain_selection_csv_path(uint32_t season, char* out, size_t out_size);
int kbo_captain_selection_csv_exists(uint32_t season);
int kbo_captain_write_selection_csv(
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source,
    char* out_path,
    size_t out_path_size);
int kbo_captain_select_for_preseason(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    KboCaptainSelectionRow* rows,
    int max_rows,
    int* out_selected_count);
DWORD WINAPI kbo_captain_preseason_selection_thread(LPVOID parameter);

#endif
