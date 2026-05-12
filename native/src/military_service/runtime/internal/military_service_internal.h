#ifndef NATIVE_SRC_MILITARY_SERVICE_MILITARY_SERVICE_C_INTERNAL_H
#define NATIVE_SRC_MILITARY_SERVICE_MILITARY_SERVICE_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../amateur_player_quality/api/amateur_player_quality.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/news/history_stubs/core_history_stubs.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/season/opening_day_storyline_guard.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../fa_market_classification/api/fa_market_classification.h"
#include "../../../fa_requalification/fa_requalification.h"
#include "../../../foreign/replacement_seed/api/foreign_replacement_seed.h"
#include "../../../team/assignment/assignment/team_assignment.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../players/loans/military_active_loan.h"
#include "../../players/state/military_player_state.h"
#include "../../returns/military_return.h"
#include "../../seed/registry/military_seed_registry.h"
#include "../../calendar/military_service_date.h"
#include "../../seed/parse/military_service_seed_parse.h"
#include "../../players/team_policy/military_service_team_policy.h"
#include "../days_tick/military_service_tick.h"

extern LONG g_military_days_tick_started;
extern LONG g_military_days_tick_log_count;
extern LONG g_military_daily_mutation_ready_log_count;
extern LONG g_military_seed_bootstrap_started;
extern LONG g_military_seed_expired_skip_log_count;

int kbo_military_daily_roster_mutation_window_ready(
    uint32_t today_serial,
    int32_t player_count);
int kbo_apply_military_service_seed_assignments(uint8_t* sang, uint8_t* kpb, const char* source);
int kbo_release_invalid_military_service_team_assignment(
    uint8_t* player,
    uint8_t* service_team,
    uint32_t service_team_id,
    const char* source,
    uint32_t vector_offset);
int kbo_tick_military_service_days(const char* source, int* out_seeded_assignments);
DWORD WINAPI kbo_military_days_tick_thread(LPVOID parameter);
DWORD WINAPI kbo_military_seed_bootstrap_thread(LPVOID parameter);
void start_kbo_military_seed_bootstrap_thread(void);
void start_kbo_military_days_tick_thread(void);

#endif
