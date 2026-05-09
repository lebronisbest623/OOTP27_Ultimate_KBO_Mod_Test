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
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
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

#include "../internal/military_service_internal.h"
/* Military service daily tick state. */

LONG g_military_days_tick_started = 0;
LONG g_military_days_tick_log_count = 0;
LONG g_military_daily_mutation_ready_log_count = 0;
LONG g_military_seed_bootstrap_started = 0;
LONG g_military_seed_expired_skip_log_count = 0;

