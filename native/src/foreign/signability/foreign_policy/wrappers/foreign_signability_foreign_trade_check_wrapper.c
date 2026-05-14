#include "../internal/foreign_signability_internal.h"

/* League trade validation hook wrapper. Included from native/KBOFix.c. */

#define KBO_TRADE_CHECK_RESULT_FOREIGN_QUOTA_BLOCK (-9)

__declspec(noinline) int ootp_kbo_trade_check_foreign_policy_probe(
    uintptr_t trade_ptr,
    int32_t side)
{
    if (kbo_fix_enabled() && kbo_custom_foreign_policy_enabled()) {
        int blocked_side = -1;
        uint32_t team_id = 0u;
        uint32_t incoming_player_id = 0u;
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        int allowed = kbo_custom_foreign_policy_trade_allows(
            trade_ptr,
            side,
            &blocked_side,
            &team_id,
            &incoming_player_id,
            &effective_before,
            &effective_after,
            &effective_limit);
        if (!allowed) {
            static volatile LONG trade_block_log_count = 0;
            LONG slot = InterlockedIncrement(&trade_block_log_count);
            if (slot <= 200) {
                kbo_log_runtimef(
                    "custom foreign policy trade blocked trade=%p request_side=%d blocked_side=%d team=%u incoming_player=%u effective_before=%u effective_after=%u limit=%u",
                    (void*)trade_ptr,
                    side,
                    blocked_side,
                    team_id,
                    incoming_player_id,
                    effective_before,
                    effective_after,
                    effective_limit);
            }
            return 0;
        }
    }
    return 1;
}

