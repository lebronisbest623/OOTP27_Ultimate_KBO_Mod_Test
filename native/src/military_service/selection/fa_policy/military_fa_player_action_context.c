#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "military_fa_player_action_context.h"
#include "../events/policy/military_selection_policy.h"
#include "../../players/team_policy/military_service_team_policy.h"

uint8_t* kbo_military_player_action_context_find_player(
    uintptr_t action_context,
    uint32_t* out_offset,
    uint32_t* out_player_id)
{
    if (out_offset != NULL) { *out_offset = 0xffffffffu; }
    if (out_player_id != NULL) { *out_player_id = 0u; }
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    uint32_t scan_bytes = (uint32_t)policy->fa_player_action_context_scan_bytes;
    if (action_context == 0) {
        return NULL;
    }

    for (uint32_t offset = 0; offset + sizeof(uintptr_t) <= scan_bytes; offset += sizeof(uintptr_t)) {
        if (!memory_range_readable((void*)(action_context + offset), sizeof(uintptr_t))) {
            continue;
        }
        uintptr_t candidate = *(uintptr_t*)(action_context + offset);
        if (!kbo_player_pointer_plausible(candidate)) {
            continue;
        }

        uint8_t* player = (uint8_t*)candidate;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || player_id > (uint32_t)policy->fa_player_id_max) {
            continue;
        }
        if (out_offset != NULL) { *out_offset = offset; }
        if (out_player_id != NULL) { *out_player_id = player_id; }
        return player;
    }
    return NULL;
}

static uint32_t kbo_military_player_action_human_manager_team_id(uintptr_t candidate)
{
    if (candidate == 0
            || !memory_range_readable((void*)candidate, OOTP27_HUMAN_MANAGER_READABLE_BYTES)) {
        return 0u;
    }

    uintptr_t vtable = *(uintptr_t*)candidate;
    if (vtable < 0x10000u || !memory_range_readable((void*)vtable, sizeof(uintptr_t))) {
        return 0u;
    }

    uint32_t team_id = *(uint32_t*)(candidate + OOTP27_HUMAN_MANAGER_CONTROLLED_TEAM_OFFSET);
    return kbo_team_id_is_military_service_team(team_id) ? team_id : 0u;
}

static uint32_t kbo_military_candidate_team_id(uintptr_t candidate)
{
    uint32_t manager_team_id = kbo_military_player_action_human_manager_team_id(candidate);
    if (manager_team_id != 0u) {
        return manager_team_id;
    }
    if (candidate == 0
            || !memory_range_readable((void*)candidate, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_team_ptr_is_military_service_team((uint8_t*)candidate)) {
        return 0u;
    }
    return *(uint32_t*)(candidate + OOTP27_KBO_TEAM_ID_OFFSET);
}

uint32_t kbo_military_fa_context_find_team_id(
    uintptr_t action_context,
    uint32_t* out_offset,
    uintptr_t* out_team_ptr)
{
    if (out_offset != NULL) { *out_offset = 0xffffffffu; }
    if (out_team_ptr != NULL) { *out_team_ptr = 0; }
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    uint32_t scan_bytes = (uint32_t)policy->fa_player_action_context_scan_bytes;
    if (action_context == 0) {
        return 0u;
    }

    uint32_t direct_team_id = kbo_military_candidate_team_id(action_context);
    if (direct_team_id != 0u) {
        if (out_offset != NULL) { *out_offset = 0u; }
        if (out_team_ptr != NULL) { *out_team_ptr = action_context; }
        return direct_team_id;
    }

    for (uint32_t offset = 0; offset + sizeof(uintptr_t) <= scan_bytes; offset += sizeof(uintptr_t)) {
        if (!memory_range_readable((void*)(action_context + offset), sizeof(uintptr_t))) {
            continue;
        }
        uintptr_t candidate = *(uintptr_t*)(action_context + offset);
        uint32_t team_id = kbo_military_candidate_team_id(candidate);
        if (team_id == 0u) {
            continue;
        }
        if (out_offset != NULL) { *out_offset = offset; }
        if (out_team_ptr != NULL) { *out_team_ptr = candidate; }
        return team_id;
    }

    uint32_t nested_scan_bytes = scan_bytes < 0x300u ? scan_bytes : 0x300u;
    int nested_blocks = 0;
    for (uint32_t offset = 0; offset + sizeof(uintptr_t) <= scan_bytes; offset += sizeof(uintptr_t)) {
        if (nested_blocks >= 48) {
            break;
        }
        if (!memory_range_readable((void*)(action_context + offset), sizeof(uintptr_t))) {
            continue;
        }

        uintptr_t block = *(uintptr_t*)(action_context + offset);
        if (block == 0
                || kbo_player_pointer_plausible(block)
                || !memory_range_readable((void*)block, sizeof(uintptr_t))) {
            continue;
        }
        nested_blocks++;

        for (uint32_t nested = 0; nested + sizeof(uintptr_t) <= nested_scan_bytes; nested += sizeof(uintptr_t)) {
            if (!memory_range_readable((void*)(block + nested), sizeof(uintptr_t))) {
                continue;
            }
            uintptr_t candidate = *(uintptr_t*)(block + nested);
            uint32_t team_id = kbo_military_candidate_team_id(candidate);
            if (team_id == 0u) {
                continue;
            }
            if (out_offset != NULL) { *out_offset = offset; }
            if (out_team_ptr != NULL) { *out_team_ptr = candidate; }
            return team_id;
        }
    }

    return 0u;
}

uint32_t kbo_military_player_action_context_find_team_id(
    uintptr_t action_context,
    uint32_t* out_offset,
    uintptr_t* out_team_ptr)
{
    return kbo_military_fa_context_find_team_id(action_context, out_offset, out_team_ptr);
}
