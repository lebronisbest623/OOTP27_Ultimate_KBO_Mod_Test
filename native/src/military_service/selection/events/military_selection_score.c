#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "military_selection_event.h"
#include "policy/military_selection_policy.h"

static int16_t kbo_military_read_player_i16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || offset + sizeof(int16_t) > OOTP27_PLAYER_SCAN_BYTES
            || !memory_range_readable(player + offset, sizeof(int16_t))) {
        return 0;
    }
    return *(int16_t*)(player + offset);
}

int kbo_military_draft_candidate_score(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return INT32_MIN;
    }
    int32_t overall = kbo_military_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_military_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_military_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_military_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    int score =
        talent * policy->draft_score_talent_weight
        + overall * policy->draft_score_overall_weight
        + ratings * policy->draft_score_ratings_weight
        + career * policy->draft_score_career_weight;

    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if ((int32_t)age >= policy->draft_score_ideal_age_min
            && (int32_t)age <= policy->draft_score_ideal_age_max) {
        score += policy->draft_score_ideal_age_base_bonus
            - ((int32_t)age - policy->draft_score_ideal_age_min) * policy->draft_score_ideal_age_step_penalty;
    } else if ((int32_t)age < policy->draft_score_ideal_age_min) {
        score += policy->draft_score_young_base_bonus
            - (policy->draft_score_ideal_age_min - (int32_t)age) * policy->draft_score_young_step_penalty;
    } else {
        score += policy->draft_score_old_base_bonus
            - ((int32_t)age - policy->draft_score_ideal_age_max) * policy->draft_score_old_step_penalty;
    }
    score += (int)(*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) % (uint32_t)policy->draft_score_player_id_mod);
    return score;
}

