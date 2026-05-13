#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/injury/api/foreign_injury_labels.h"
#include "../../../foreign/rights/query/foreign_waiver_rights_query.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../assignment/org_query/team_org_assignment_query.h"
#include "../../lookup/team_lookup.h"
#include "../internal/team_add_player_guard_internal.h"

int kbo_team_add_foreign_policy_should_block(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t team_id,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t caller_rva)
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

    uint32_t today = 0u;
    if (kbo_get_current_yyyymmdd(&today) && today != 0u) {
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t holder_team_id = 0u;
        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
                && holder_team_id != 0u) {
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            uint32_t original_team_id = 0u;
            if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
                original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
            }
            int score = kbo_foreign_waiver_value_score(player);

            if (holder_team_id == team_id) {
                uint32_t effective_before = 0u;
                uint32_t effective_after = 0u;
                uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
                uint8_t slot_type = 0u;
                uint32_t injured_player_id = 0u;
                int quota_allowed = kbo_custom_foreign_policy_team_allows_final_signing(
                    team_id,
                    player,
                    &effective_before,
                    &effective_after,
                    &effective_limit,
                    &slot_type,
                    &injured_player_id);

                static volatile LONG reserve_holder_allow_log_count = 0;
                LONG allow_slot = InterlockedIncrement(&reserve_holder_allow_log_count);
                if (allow_slot <= 200) {
                    append_logf(
                        "foreign retention re-signing: holder_quota_check team=%u player=%u today=%u before_current=%u before_active=%u current=%u active=%u original=%u score=%d quota_allowed=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u caller_rva=0x%x",
                        team_id,
                        player_id,
                        today,
                        before_current_team_id,
                        before_active_team_id,
                        current_team_id,
                        active_team_id,
                        original_team_id,
                        score,
                        quota_allowed,
                        effective_before,
                        effective_after,
                        effective_limit,
                        slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                        injured_player_id,
                        caller_rva);
                }
                if (quota_allowed) {
                    return 0;
                }
                return 1;
            }

            static volatile LONG reserve_block_log_count = 0;
            LONG reserve_slot = InterlockedIncrement(&reserve_block_log_count);
            if (reserve_slot <= 200) {
                append_logf(
                    "foreign retention re-signing: blocked_non_holder_team_add team=%u holder_team=%u player=%u today=%u before_current=%u before_active=%u current=%u active=%u original=%u score=%d caller_rva=0x%x",
                    team_id,
                    holder_team_id,
                    player_id,
                    today,
                    before_current_team_id,
                    before_active_team_id,
                    current_team_id,
                    active_team_id,
                    original_team_id,
                    score,
                    caller_rva);
            }
            return 1;
        }
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    int allowed = kbo_custom_foreign_policy_team_allows_final_signing(
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
