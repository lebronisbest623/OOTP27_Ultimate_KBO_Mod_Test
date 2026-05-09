#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../internal/intl_established_fa_postscan_internal.h"

#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_DELAY_MS 4500ull
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS 2500ull
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES 8
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_DETAIL_LOGS 160
#define KBO_INTL_ESTABLISHED_FA_ASIAN_STARTER_SCORE_CAP 72000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_BULLPEN_SCORE_CAP 70000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_UNKNOWN_PITCHER_SCORE_CAP 71000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_HITTER_SCORE_CAP 72000
#define KBO_INTL_ESTABLISHED_FA_ASIAN_CATCHER_SCORE_CAP 65000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_STARTER_SCORE_CAP 115000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_BULLPEN_SCORE_CAP 95000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_UNKNOWN_PITCHER_SCORE_CAP 105000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_HITTER_SCORE_CAP 110000
#define KBO_INTL_ESTABLISHED_FA_NON_ASIAN_CATCHER_SCORE_CAP 80000

typedef struct KboIntlEstablishedFaPostscanState {
    LONG pending;
    LONG batch_id;
    int32_t before_count;
    uint32_t before_max_player_id;
    int32_t original_count;
    int32_t expected_count;
    int multiplier;
    uint32_t primary_league_id;
    uint32_t fallback_league_id;
    uint32_t scheduled_date;
    ULONGLONG due_tick;
    int attempts;
} KboIntlEstablishedFaPostscanState;

KboIntlEstablishedFaPostscanState g_kbo_intl_established_fa_postscan = {0};
volatile LONG g_kbo_intl_established_fa_postscan_worker_started = 0;

const char* kbo_intl_established_fa_quality_policy_label(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role)
{
    if (asian_quota) {
        if (position_group == 1u) {
            if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                return "asian_starter_cap";
            }
            if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                return "asian_bullpen_cap";
            }
            return "asian_pitcher_cap";
        }
        if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
            return "asian_catcher_cap";
        }
        return "asian_hitter_cap";
    }

    if (position_group == 1u) {
        if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
            return "non_asian_starter_cap";
        }
        if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
            return "non_asian_bullpen_cap";
        }
        return "non_asian_pitcher_cap";
    }
    if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
        return "non_asian_catcher_cap";
    }
    return "non_asian_hitter_cap";
}

