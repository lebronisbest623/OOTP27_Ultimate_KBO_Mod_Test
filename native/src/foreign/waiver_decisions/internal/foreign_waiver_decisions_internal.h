#ifndef NATIVE_SRC_FOREIGN_FOREIGN_WAIVER_DECISIONS_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_FOREIGN_WAIVER_DECISIONS_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../common/csv/foreign_csv_parse.h"
#include "../../waiver_core/api/foreign_waiver_core.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../api/foreign_waiver_decisions.h"
#include "../../common/paths/foreign_waiver_paths.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../rights/query/foreign_waiver_rights_query.h"


#define WIN32_LEAN_AND_MEAN

extern LONG g_kbo_foreign_waiver_decision_lock;

uint32_t kbo_get_player_original_team_id(uint8_t* player);
uint32_t kbo_get_foreign_waiver_decision_team_id(uint8_t* player);
int kbo_original_club_priority_window_allows(uint8_t* player, uint32_t team_id, const char* action_name);
int kbo_retain_foreign_player_rights(
    uint8_t* player,
    uint8_t* retaining_team,
    uint32_t fallback_league_id,
    uint32_t player_id,
    uint32_t team_id);
int kbo_append_foreign_waiver_decision_record(
    const char* source,
    const char* action,
    uint32_t team_id,
    uint32_t player_id,
    int score,
    int forced,
    int executed);
int kbo_foreign_waiver_decision_exists(uint32_t window_end, uint32_t team_id, uint32_t player_id);
int kbo_foreign_waiver_latest_decision_action(
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id,
    char* out_action,
    size_t out_action_size);
int kbo_execute_foreign_waiver_claim(const char* line, int line_no);
int get_kbo_foreign_waiver_cmd_path(char* out, size_t out_size);
int kbo_append_foreign_waiver_cmd_line(const char* line);
int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain);
void process_foreign_waiver_commands(void);

#endif
