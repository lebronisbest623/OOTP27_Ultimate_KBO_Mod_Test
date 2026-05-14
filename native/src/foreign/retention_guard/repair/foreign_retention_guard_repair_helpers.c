#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "foreign_retention_guard_repair_helpers.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"

static volatile LONG g_kbo_foreign_retention_guard_repair_log_count = 0;

static int kbo_foreign_retention_guard_clear_restricted_marks(
    uint8_t* player,
    uint8_t* team,
    uint32_t player_id)
{
    int changed = 0;
    if (player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u) {
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u) {
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u) {
        player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
        changed = 1;
    }
    if (team != NULL
            && kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id) > 0) {
        changed = 1;
    }
    return changed;
}

static uint32_t kbo_foreign_retention_guard_record_league(
    uint8_t* team,
    const KboForeignRetentionGuardRecord* rec)
{
    uint32_t league_id = rec != NULL ? rec->league_id : 0u;
    if (league_id == 0u && team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    return league_id;
}

static int kbo_foreign_retention_guard_apply_contract_snapshot(
    uint8_t* player,
    const KboForeignRetentionGuardRecord* rec)
{
    if (player == NULL || rec == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    int changed = 0;
    int salary_missing = 0;
    int salaries_readable = memory_range_readable(
        player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
        OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t));
    int32_t* salaries = salaries_readable
        ? (int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET)
        : NULL;
    if (salaries != NULL && rec->contract_years[0] > 0 && salaries[0] == 0) {
        salary_missing = 1;
    }
    if (rec->contract_status != 0u
            && memory_range_readable(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET, sizeof(uint32_t))) {
        uint32_t* status = (uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
        if (*status == 0u || salary_missing) {
            *status = rec->contract_status;
            changed = 1;
        }
    }
    if (rec->contract_start_year != 0u
            && memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(uint32_t))) {
        uint32_t* start_year = (uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
        if (*start_year == 0u || salary_missing) {
            *start_year = rec->contract_start_year;
            changed = 1;
        }
    }
    if (salaries != NULL) {
        for (uint32_t year = 0u; year < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; year++) {
            if (rec->contract_years[year] > 0 && salaries[year] == 0) {
                salaries[year] = rec->contract_years[year];
                changed = 1;
            }
        }
    }
    return changed;
}

int kbo_foreign_retention_guard_stamp_signed_state(
    uint8_t* player,
    uint8_t* team,
    const KboForeignRetentionGuardRecord* rec)
{
    if (player == NULL || team == NULL || rec == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t league_id = kbo_foreign_retention_guard_record_league(team, rec);
    if (league_id == 0u) {
        return 0;
    }

    int changed = 0;
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != rec->team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = rec->team_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != rec->team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = rec->team_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            && *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) != rec->team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = rec->team_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = rec->team_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    changed |= kbo_foreign_retention_guard_apply_contract_snapshot(player, rec);
    if (*(uint16_t*)(player + OOTP27_PLAYER_STATUS_FLAGS_OFFSET) != 0x0100u) {
        *(uint16_t*)(player + OOTP27_PLAYER_STATUS_FLAGS_OFFSET) = 0x0100u;
        changed = 1;
    }
    changed |= kbo_foreign_retention_guard_clear_restricted_marks(player, team, rec->player_id);
    kbo_add_player_id_to_team_assignment_arrays(team, rec->player_id);
    return changed;
}

int kbo_foreign_retention_guard_clean_signed_holder(
    uint8_t* player,
    uint8_t* team,
    const KboForeignRetentionGuardRecord* rec)
{
    if (player == NULL || team == NULL || rec == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t league_id = kbo_foreign_retention_guard_record_league(team, rec);
    int changed = 0;
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != rec->team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = rec->team_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != rec->team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = rec->team_id;
        changed = 1;
    }
    if (league_id != 0u
            && *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (league_id != 0u && *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = rec->team_id;
        changed = 1;
    }
    if (league_id != 0u && *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = league_id;
        changed = 1;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            && *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) != rec->team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = rec->team_id;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    changed |= kbo_foreign_retention_guard_apply_contract_snapshot(player, rec);
    changed |= kbo_foreign_retention_guard_clear_restricted_marks(player, team, rec->player_id);
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == rec->team_id) {
        kbo_add_player_id_to_team_assignment_arrays(team, rec->player_id);
    }
    return changed;
}

int kbo_foreign_retention_guard_player_repairable(
    uint8_t* player,
    const KboForeignRetentionGuardRecord* rec)
{
    if (player == NULL || rec == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
            || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    uint32_t current = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t default_team = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
        : 0u;

    return current == 0u
        && (active == 0u || active == rec->team_id)
        && (original == 0u || original == rec->team_id)
        && (default_team == 0u || default_team == rec->team_id)
        && *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == 0u;
}

void kbo_foreign_retention_guard_log_cleaned(
    const char* source,
    const KboForeignRetentionGuardRecord* rec,
    uint8_t* player,
    uint32_t today,
    uint32_t before_current,
    uint32_t before_active,
    uint32_t before_original,
    uint32_t before_default,
    uint32_t before_league,
    uint8_t before_restricted,
    uint8_t before_secondary,
    uint8_t before_contract_level)
{
    LONG repair_slot = InterlockedIncrement(&g_kbo_foreign_retention_guard_repair_log_count);
    if (repair_slot > 200) {
        return;
    }
    kbo_log_runtimef(
        "foreign retention guard: cleaned signed holder source=%s player=%u team=%u today=%u before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_restricted=%u before_secondary=%u before_contract_level=%u after_current=%u after_active=%u after_default=%u after_contract_level=%u",
        source != NULL ? source : "",
        rec->player_id,
        rec->team_id,
        today,
        before_current,
        before_active,
        before_original,
        before_default,
        before_league,
        (uint32_t)before_restricted,
        (uint32_t)before_secondary,
        (uint32_t)before_contract_level,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
            : 0u,
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET]);
}

void kbo_foreign_retention_guard_log_restored(
    const char* source,
    const KboForeignRetentionGuardRecord* rec,
    uint8_t* player,
    uint32_t today,
    uint32_t before_current,
    uint32_t before_active,
    uint32_t before_original,
    uint32_t before_default,
    uint32_t before_league,
    uint8_t before_restricted,
    uint8_t before_secondary,
    uint8_t before_contract_level)
{
    LONG repair_slot = InterlockedIncrement(&g_kbo_foreign_retention_guard_repair_log_count);
    if (repair_slot > 200) {
        return;
    }
    int32_t salary_y1 = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, sizeof(int32_t))
        ? *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET)
        : 0;
    kbo_log_runtimef(
        "foreign retention guard: restored cleared holder signing source=%s player=%u team=%u league=%u today=%u signed_on=%u expires_on=%u before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_restricted=%u before_secondary=%u before_contract_level=%u after_current=%u after_active=%u after_original=%u after_default=%u after_league=%u after_contract_level=%u salary_y1=%d",
        source != NULL ? source : "",
        rec->player_id,
        rec->team_id,
        rec->league_id,
        today,
        rec->signed_on_yyyymmdd,
        rec->expires_on_yyyymmdd,
        before_current,
        before_active,
        before_original,
        before_default,
        before_league,
        (uint32_t)before_restricted,
        (uint32_t)before_secondary,
        (uint32_t)before_contract_level,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
        memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
            : 0u,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
        salary_y1);
}
