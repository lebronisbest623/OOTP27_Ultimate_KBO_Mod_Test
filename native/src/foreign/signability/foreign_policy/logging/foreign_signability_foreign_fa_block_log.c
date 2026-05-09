#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/lookup/team_lookup.h"
#include "../../../common/dates/foreign_waiver_date.h"
#include "../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../common/policy/foreign_waiver_policy.h"
#include "../../../injury/api/foreign_injury.h"
#include "../../../rights/query/foreign_waiver_rights_query.h"
#include "../../../../military_service/military_service.h"
#include "../../state/foreign_fa_block_state.h"

/* Foreign FA/signability diagnostic logging. */

void kbo_log_foreign_signability_block_callsite(
    uintptr_t caller_rva,
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today)
{
    enum { KBO_SIGNABILITY_CALLSITE_LOG_SLOTS = 64 };
    static volatile LONG logged_count = 0;
    static uintptr_t logged_callers[KBO_SIGNABILITY_CALLSITE_LOG_SLOTS] = {0};

    if (caller_rva == 0) {
        return;
    }
    for (int i = 0; i < KBO_SIGNABILITY_CALLSITE_LOG_SLOTS; i++) {
        if (logged_callers[i] == caller_rva) {
            return;
        }
    }

    LONG slot = InterlockedIncrement(&logged_count) - 1;
    if (slot < 0 || slot >= KBO_SIGNABILITY_CALLSITE_LOG_SLOTS) {
        return;
    }
    logged_callers[slot] = caller_rva;
    append_logf(
        "foreign reserve signability: new block callsite caller_rva=0x%llx player=%u requester_team=%u holder_team=%u original=%d today=%u",
        (unsigned long long)caller_rva,
        player_id,
        requester_team_id,
        holder_team_id,
        original_signability,
        today);
}

