#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../internal/team_add_player_guard_internal.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/injury/api/foreign_injury.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../lookup/team_lookup.h"

void kbo_team_add_attach_foreign_injury_replacement_success(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    const char* source)
{
    if (!kbo_fix_enabled()
            || team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (team_id == 0u
            || team_league_id == 0u
            || (kbo_league_id != 0u && team_league_id != kbo_league_id)) {
        return;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    (void)kbo_custom_foreign_policy_team_allows_final_signing(
        team_id,
        player,
        &effective_before,
        &effective_after,
        &effective_limit,
        &slot_type,
        &injured_player_id);
    if (slot_type == 0u || injured_player_id == 0u) {
        return;
    }

    kbo_attach_foreign_injury_replacement_after_signing(
        team_id,
        player,
        slot_type,
        injured_player_id,
        source != NULL ? source : "team_add_player_success");
}
