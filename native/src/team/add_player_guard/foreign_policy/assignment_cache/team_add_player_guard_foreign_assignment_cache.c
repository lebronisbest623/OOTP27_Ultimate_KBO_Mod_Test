#include "../../internal/team_add_player_guard_internal.h"
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/quota/counts/foreign_quota_counts.h"
#include "../../../../runtime_memory/runtime_memory.h"

void kbo_team_add_note_foreign_assignment_success(
    uint8_t* player,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_loan_team_id)
{
    uint32_t nation_id = player != NULL
        && memory_range_readable(player + OOTP27_PLAYER_NATION_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET)
        : 0u;
    if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return;
    }

    uint32_t player_id = memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
        : 0u;
    uint32_t after_current_team_id = memory_range_readable(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)
        : 0u;
    uint32_t after_active_team_id = memory_range_readable(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET)
        : 0u;
    uint32_t after_loan_team_id = memory_range_readable(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET)
        : 0u;
    kbo_foreign_org_count_cache_note_player_assignment_change(
        before_current_team_id,
        before_active_team_id,
        before_loan_team_id,
        after_current_team_id,
        after_active_team_id,
        after_loan_team_id,
        player_id,
        kbo_player_is_asian_quota_candidate(player));
}