#include "allstar_candidate_seed_helpers.h"

#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../allstar_league_context/allstar_league_context.h"
#include "../team_patch/allstar_team_patch.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"

int kbo_allstar_candidate_seed_is_exhibition_team(uint8_t* team)
{
    if (team == NULL) {
        return 0;
    }

    return team_has_ootp_string_text(team, "All-Stars")
        || team_has_ootp_string_text(team, "Future Stars")
        || team_has_ootp_string_text(team, "AS1")
        || team_has_ootp_string_text(team, "AS2")
        || team_has_ootp_string_text(team, "FS1")
        || team_has_ootp_string_text(team, "FS2");
}

static uint8_t* kbo_allstar_candidate_find_team_by_id_in_league(
    uint32_t team_id,
    uint32_t league_id)
{
    if (team_id == 0u) {
        return NULL;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return NULL;
    }

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) != team_id) {
            continue;
        }
        if (kbo_allstar_candidate_seed_is_exhibition_team(team)) {
            continue;
        }
        if (league_id != 0u && !kbo_allstar_team_matches_league_ids(team, league_id, 0u)) {
            continue;
        }
        return team;
    }

    return NULL;
}

static uint8_t kbo_allstar_candidate_player_side_for_team_id(
    uint32_t team_id,
    uint32_t league_id,
    uint32_t league_year)
{
    if (team_id == 0u) {
        return 0;
    }

    uint8_t* team = kbo_allstar_candidate_find_team_by_id_in_league(team_id, league_id);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    return kbo_allstar_side_for_team(team, league_year);
}

uint8_t kbo_allstar_candidate_player_side(
    uint8_t* player,
    uint32_t league_id,
    uint32_t league_year,
    uint32_t* out_team_id)
{
    static const uint32_t team_id_offsets[] = {
        OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET,
        OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET,
        OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET
    };

    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    for (int i = 0; i < (int)(sizeof(team_id_offsets) / sizeof(team_id_offsets[0])); i++) {
        uint32_t offset = team_id_offsets[i];
        if (!memory_range_readable(player + offset, sizeof(uint32_t))) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(player + offset);
        uint8_t side = kbo_allstar_candidate_player_side_for_team_id(team_id, league_id, league_year);
        if (side == 1u || side == 2u) {
            if (out_team_id != NULL) {
                *out_team_id = team_id;
            }
            return side;
        }
    }

    return 0;
}
