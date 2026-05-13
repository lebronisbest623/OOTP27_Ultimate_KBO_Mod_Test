#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../assignment/assignment/team_assignment.h"
#include "../../lookup/team_lookup.h"
#include "team_add_player_guard_foreign_purchase_restore.h"

int kbo_team_add_restore_source_team_after_blocked_foreign_purchase(
    uint8_t* player,
    uint32_t source_team_id,
    uint32_t blocked_team_id,
    uint32_t caller_rva)
{
    if (player == NULL
            || source_team_id == 0u
            || blocked_team_id == 0u
            || source_team_id == blocked_team_id
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (player_id == 0u || current_team_id != 0u || active_team_id != 0u) {
        return 0;
    }

    uint8_t* source_team = find_kbo_team_by_numeric_id_any_league(source_team_id, 1);
    if (source_team == NULL
            || !memory_range_readable(source_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t source_league_id = *(uint32_t*)(source_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint8_t* blocked_team = find_kbo_team_by_numeric_id_any_league(blocked_team_id, 1);
    uint32_t blocked_league_id = blocked_team != NULL
            && memory_range_readable(blocked_team, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? *(uint32_t*)(blocked_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET)
        : 0u;
    if (source_league_id != 0u
            && blocked_league_id != 0u
            && source_league_id == blocked_league_id) {
        return 0;
    }

    int called_pre_change = 0;
    int called_register = 0;
    int called_attach = 0;
    kbo_assign_player_to_team_internal(
        player,
        source_team,
        source_league_id,
        1,
        &called_pre_change,
        &called_register,
        &called_attach);

    uint32_t restored_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t restored_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (restored_current_team_id != source_team_id) {
        append_logf(
            "custom foreign policy blocked purchase source restore failed player=%u blocked_team=%u source_team=%u source_league=%u caller_rva=0x%x restored_current=%u restored_active=%u",
            player_id,
            blocked_team_id,
            source_team_id,
            source_league_id,
            caller_rva,
            restored_current_team_id,
            restored_active_team_id);
        return 0;
    }

    append_logf(
        "custom foreign policy blocked purchase restored source player=%u blocked_team=%u source_team=%u source_league=%u caller_rva=0x%x restored_current=%u restored_active=%u pre=%d register=%d attach=%d",
        player_id,
        blocked_team_id,
        source_team_id,
        source_league_id,
        caller_rva,
        restored_current_team_id,
        restored_active_team_id,
        called_pre_change,
        called_register,
        called_attach);
    return 1;
}
