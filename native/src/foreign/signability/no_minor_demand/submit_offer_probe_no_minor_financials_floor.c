#include "../submit_offer_probe/internal/submit_offer_probe_internal.h"

int32_t kbo_no_minor_resolve_current_league_id(void)
{
    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0
            || !memory_range_readable(
                (void*)(global_db + OOTP27_GLOBAL_CURRENT_LEAGUE_OFFSET),
                sizeof(uintptr_t))) {
        uint32_t fallback_league_id = kbo_get_foreign_waiver_league_id();
        if (fallback_league_id == 0u) {
            fallback_league_id = kbo_resolve_kbo_league_id();
        }
        return fallback_league_id > 0u && fallback_league_id <= 100000u
            ? (int32_t)fallback_league_id
            : 0;
    }

    uintptr_t current_league_context = *(uintptr_t*)(global_db + OOTP27_GLOBAL_CURRENT_LEAGUE_OFFSET);
    if (current_league_context == 0
            || !memory_range_readable(
                (void*)(current_league_context + OOTP27_GLOBAL_CURRENT_LEAGUE_ID_OFFSET),
                sizeof(int32_t))) {
        uint32_t fallback_league_id = kbo_get_foreign_waiver_league_id();
        if (fallback_league_id == 0u) {
            fallback_league_id = kbo_resolve_kbo_league_id();
        }
        return fallback_league_id > 0u && fallback_league_id <= 100000u
            ? (int32_t)fallback_league_id
            : 0;
    }

    int32_t league_id = *(int32_t*)(current_league_context + OOTP27_GLOBAL_CURRENT_LEAGUE_ID_OFFSET);
    if (league_id <= 0 || league_id > 100000) {
        uint32_t fallback_league_id = kbo_get_foreign_waiver_league_id();
        if (fallback_league_id == 0u) {
            fallback_league_id = kbo_resolve_kbo_league_id();
        }
        return fallback_league_id > 0u && fallback_league_id <= 100000u
            ? (int32_t)fallback_league_id
            : 0;
    }
    return league_id;
}

int32_t kbo_no_minor_resolve_current_league_minimum_salary(void)
{
    uint8_t* financials = kbo_resolve_current_league_financials(NULL);
    if (financials == NULL
            || !memory_range_readable(
                financials + OOTP27_LEAGUE_MINIMUM_SALARY_OFFSET,
                sizeof(int32_t))) {
        return 0;
    }

    int32_t minimum_salary = *(int32_t*)(financials + OOTP27_LEAGUE_MINIMUM_SALARY_OFFSET);
    if (minimum_salary <= 0 || minimum_salary > 1000000000) {
        return 0;
    }
    return minimum_salary;
}

int32_t kbo_no_minor_current_league_minimum_salary(void)
{
    return kbo_no_minor_resolve_current_league_minimum_salary();
}

int kbo_no_minor_clamp_player_demand_salary(uintptr_t player_ptr, uintptr_t screen_ptr, const char* source)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 0, 0) == 0) {
        return 0;
    }
    kbo_prepare_foreign_fa_offer_demand_baseline(player_ptr, source);
    kbo_log_financials_salary_baseline_probe(source);
    if (player_ptr == 0 || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
        : 0u;

    int32_t salary_floor = kbo_no_minor_resolve_current_league_minimum_salary();
    if (salary_floor <= 0) {
        static LONG floor_miss_log_count = 0;
        LONG miss_slot = InterlockedIncrement(&floor_miss_log_count);
        if (miss_slot <= 40) {
            append_logf(
                "KBO no-minor demand floor skipped: source=%s screen=%p player=%u reason=no_floor",
                source,
                (void*)screen_ptr,
                player_id);
        }
        return 0;
    }

    uint32_t league_id = (uint32_t)kbo_no_minor_resolve_current_league_id();
    if (!kbo_no_minor_player_is_teamless_demand_floor_candidate(player, league_id)) {
        return 0;
    }

    int32_t old_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    uint8_t old_contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    int changed = 0;
    if (old_demand < salary_floor) {
        *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = salary_floor;
        changed = 1;
    }

    if (changed) {
        static LONG clamp_log_count = 0;
        LONG clamp_slot = InterlockedIncrement(&clamp_log_count);
        if (clamp_slot <= 120) {
            append_logf(
                "KBO no-minor demand floor applied: source=%s screen=%p player=%u old_demand=%d floor=%d observed_contract_level=%u",
                source,
                (void*)screen_ptr,
                player_id,
                old_demand,
                salary_floor,
                (unsigned)old_contract_level);
        }
    }
    return changed;
}

int kbo_no_minor_clamp_offer_screen_player_salary(uintptr_t screen_ptr, const char* source)
{
    if (screen_ptr == 0 || !memory_range_readable((void*)screen_ptr, 0xc0)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL) {
        return 0;
    }

    return kbo_no_minor_clamp_player_demand_salary((uintptr_t)player, screen_ptr, source);
}
