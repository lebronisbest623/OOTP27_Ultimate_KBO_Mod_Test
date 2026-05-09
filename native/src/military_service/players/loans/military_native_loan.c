#include "military_native_loan.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

/* ---- Native loan helpers ---- */

int kbo_player_native_on_loan(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) > 0
        && player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0
        && player[OOTP27_PLAYER_DFA_FLAG_OFFSET] == 0
        && player[OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET] != 0;
}

int kbo_clear_native_player_loan(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0;
    player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] = 1;
    return !kbo_player_native_on_loan(player);
}
