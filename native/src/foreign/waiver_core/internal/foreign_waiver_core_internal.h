#ifndef NATIVE_SRC_FOREIGN_FOREIGN_WAIVER_CORE_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_FOREIGN_WAIVER_CORE_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/events/core_league_events.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../../team/names/team_string.h"
#include "../../common/csv/foreign_csv_parse.h"
#include "../../common/events/foreign_priority_events.h"
#include "../../common/config/foreign_waiver_config.h"
#include "../api/foreign_waiver_core.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../waiver_decisions/api/foreign_waiver_decisions.h"
#include "../../common/events/foreign_waiver_events.h"
#include "../../common/paths/foreign_waiver_paths.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../injury/api/foreign_injury_labels.h"
#include "../../replacement_seed/api/foreign_replacement_seed.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../../roster_audit/api/foreign_roster_audit.h"

#define KBO_FOREIGN_WAIVER_AI_AUTO_TEAM_SLOT_MAX 512





#define WIN32_LEAN_AND_MEAN
#define KBO_FOREIGN_WAIVER_AI_AUTO_TEAM_SLOT_MAX 512
typedef struct KboForeignWaiverAiTargetCandidate {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t current_team_id;
    int score;
    int forced;
} KboForeignWaiverAiTargetCandidate;

extern LONG g_kbo_foreign_waiver_scanner_started;

int kbo_apply_ai_foreign_waiver_rules(
    uint32_t player_id,
    uint32_t player_current_team_id,
    int value_score,
    int forced,
    uint32_t target_team_id);
int kbo_ai_foreign_waiver_should_retain(
    uint8_t* player,
    uint32_t player_id,
    uint32_t decision_team_id,
    int value_score,
    int forced,
    int32_t* out_threshold,
    const char** out_reason);
void run_foreign_waiver_ai_core_once(void);
int get_kbo_foreign_waiver_csv_path(char* out, size_t out_size);
int append_foreign_waiver_candidate_csv_header(HANDLE file);
int is_csv_empty(HANDLE file);
void write_foreign_waiver_candidates(const char* source);
int kbo_resolve_foreign_waiver_top_candidate_for_team(
    uint32_t team_id,
    uint32_t* out_player_id,
    uint32_t* out_current_team_id);
DWORD WINAPI kbo_foreign_waiver_scanner_thread(LPVOID parameter);
void start_kbo_foreign_waiver_scanner_thread(void);

#endif
