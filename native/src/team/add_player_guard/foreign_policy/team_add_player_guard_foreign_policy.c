#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/injury/api/foreign_injury_labels.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../assignment/org_query/team_org_assignment_query.h"
#include "../../lookup/team_lookup.h"
#include "../internal/team_add_player_guard_internal.h"

int kbo_team_add_foreign_policy_should_block(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t team_id,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || team_id == 0u
            || team_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    if (kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
        return 0;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    int allowed = kbo_custom_foreign_policy_team_allows_candidate(
        team_id,
        player,
        &effective_before,
        &effective_after,
        &effective_limit,
        &slot_type,
        &injured_player_id);
    if (allowed) {
        return 0;
    }

    static volatile LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 300) {
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        append_logf(
            "custom foreign policy team-add blocked player=%u team=%u before_current=%u before_active=%u current=%u active=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u",
            player_id,
            team_id,
            before_current_team_id,
            before_active_team_id,
            current_team_id,
            active_team_id,
            effective_before,
            effective_after,
            effective_limit,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id);
    }
    return 1;
}
