#include "../submit_offer_probe/internal/submit_offer_probe_internal.h"

volatile LONG g_kbo_no_minor_contract_demand_floor_scanner_started = 0;

uintptr_t* kbo_no_minor_copy_player_vector_snapshot(
    uintptr_t player_vector,
    int32_t player_count,
    const char** out_failure_reason)
{
    if (out_failure_reason != NULL) {
        *out_failure_reason = "unknown";
    }
    if (player_vector == 0 || player_count <= 0) {
        if (out_failure_reason != NULL) { *out_failure_reason = "invalid_vector"; }
        return NULL;
    }
    if ((SIZE_T)player_count > ((SIZE_T)-1 / sizeof(uintptr_t))) {
        if (out_failure_reason != NULL) { *out_failure_reason = "count_overflow"; }
        return NULL;
    }

    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        if (out_failure_reason != NULL) { *out_failure_reason = "unreadable_vector"; }
        return NULL;
    }

    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (snapshot == NULL) {
        if (out_failure_reason != NULL) { *out_failure_reason = "alloc_failed"; }
        return NULL;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        if (out_failure_reason != NULL) { *out_failure_reason = "copy_failed"; }
        return NULL;
    }

    if (out_failure_reason != NULL) {
        *out_failure_reason = NULL;
    }
    return snapshot;
}

int kbo_no_minor_player_is_teamless_demand_floor_candidate(uint8_t* player, uint32_t league_id)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (player_id == 0u || player_id > 200000000u || age < 16u || age > 60u || current_team_id != 0u) {
        return 0;
    }
    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (kbo_player_is_draft_pool_candidate(player)) {
        return 0;
    }

    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (league_id != 0u
            && current_league_id != league_id
            && draft_league_id != league_id
            && !kbo_player_has_nonzero_evaluation(player)) {
        return 0;
    }

    return 1;
}

__declspec(noinline) int32_t ootp_kbo_no_minor_demand_write_floor_probe(
    uintptr_t player_ptr,
    int32_t proposed_demand,
    uint32_t source_rva,
    int32_t salary_floor_hint)
{
    kbo_restore_foreign_fa_demand_salary_ladder("demand_write");
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 0, 0) == 0) {
        return proposed_demand;
    }
    kbo_log_financials_salary_baseline_probe("demand_write");
    if (!kbo_player_pointer_plausible(player_ptr)) {
        return proposed_demand;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t league_id = (uint32_t)kbo_no_minor_resolve_current_league_id();
    if (!kbo_no_minor_player_is_teamless_demand_floor_candidate(player, league_id)) {
        return proposed_demand;
    }

    int32_t salary_floor = salary_floor_hint;
    if (salary_floor <= 0 || salary_floor > 1000000000) {
        salary_floor = kbo_no_minor_resolve_current_league_minimum_salary();
    }
    if (salary_floor <= 0) {
        return proposed_demand;
    }

    int32_t adjusted_demand = proposed_demand < salary_floor ? salary_floor : proposed_demand;
    if (adjusted_demand != proposed_demand) {
        uint8_t observed_contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
        static LONG write_floor_log_count = 0;
        LONG slot = InterlockedIncrement(&write_floor_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO no-minor demand write floor: source=0x%x player=%u proposed=%d adjusted=%d floor=%d observed_contract_level=%u",
                source_rva,
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                proposed_demand,
                adjusted_demand,
                salary_floor,
                (unsigned)observed_contract_level);
        }
    }

    return adjusted_demand;
}

int kbo_no_minor_scan_and_floor_teamless_fa_demands(const char* source)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 0, 0) == 0) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || today == 0u) {
        static LONG no_date_log_count = 0;
        LONG slot = InterlockedIncrement(&no_date_log_count);
        if (slot <= 5) {
            append_logf("KBO no-minor demand floor scan skipped source=%s reason=current_date_unavailable", source);
        }
        return 0;
    }

    kbo_log_financials_salary_baseline_probe(source);

    int32_t salary_floor = kbo_no_minor_resolve_current_league_minimum_salary();
    if (salary_floor <= 0) {
        static LONG no_floor_log_count = 0;
        LONG slot = InterlockedIncrement(&no_floor_log_count);
        if (slot <= 20) {
            append_logf("KBO no-minor demand floor scan skipped source=%s reason=no_floor", source);
        }
        return 0;
    }

    uint32_t league_id = (uint32_t)kbo_no_minor_resolve_current_league_id();
    if (league_id == 0u) {
        static LONG no_league_log_count = 0;
        LONG slot = InterlockedIncrement(&no_league_log_count);
        if (slot <= 20) {
            append_logf("KBO no-minor demand floor scan skipped source=%s reason=no_league", source);
        }
        return 0;
    }
    uint8_t* financials = kbo_resolve_current_league_financials(NULL);

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        static LONG no_vector_log_count = 0;
        LONG slot = InterlockedIncrement(&no_vector_log_count);
        if (slot <= 20) {
            append_logf("KBO no-minor demand floor scan skipped source=%s reason=no_player_vector", source);
        }
        return 0;
    }

    const char* snapshot_failure_reason = NULL;
    uintptr_t* player_snapshot = kbo_no_minor_copy_player_vector_snapshot(
        player_vector,
        player_count,
        &snapshot_failure_reason);
    if (player_snapshot == NULL) {
        static LONG snapshot_fail_log_count = 0;
        LONG slot = InterlockedIncrement(&snapshot_fail_log_count);
        if (slot <= 20 || (slot % 100) == 0) {
            append_logf(
                "KBO no-minor demand floor scan skipped source=%s reason=player_vector_snapshot_failed detail=%s vector=%p count=%d vector_off=0x%x log_slot=%ld",
                source,
                snapshot_failure_reason != NULL ? snapshot_failure_reason : "unknown",
                (void*)player_vector,
                player_count,
                vector_offset,
                slot);
        }
        return 0;
    }

    int scanned = 0;
    int teamless = 0;
    int changed = 0;
    int demand_fixed = 0;
    int foreign_demand_mapped = 0;
    int level_observed_nonmajor = 0;
    static LONG detail_log_count = 0;
    static LONG foreign_detail_log_count = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;

        uint8_t* player = (uint8_t*)player_ptr;
        int no_minor_candidate = kbo_no_minor_player_is_teamless_demand_floor_candidate(player, league_id);
        int foreign_candidate = kbo_foreign_fa_demand_remap_candidate(player);
        if (!no_minor_candidate && !foreign_candidate) {
            continue;
        }
        if (no_minor_candidate) {
            teamless++;
        }

        if (!memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))) {
            continue;
        }

        int32_t old_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
        uint8_t old_contract_level = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET, sizeof(uint8_t))
            ? *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET)
            : 0u;
        int player_changed = 0;
        if (no_minor_candidate && old_contract_level != 1u) {
            level_observed_nonmajor++;
        }
        if (no_minor_candidate && old_demand < salary_floor) {
            *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = salary_floor;
            demand_fixed++;
            player_changed = 1;
        }
        if (foreign_candidate && financials != NULL) {
            int32_t current_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            if (kbo_foreign_fa_demand_remap_already_applied(player_id, current_demand)) {
                continue;
            }
            int asian_quota = kbo_player_is_asian_quota_candidate(player);
            int32_t mapped_demand = kbo_foreign_fa_remap_demand_from_salary_ladder(current_demand, financials, asian_quota);
            if (mapped_demand > 0 && mapped_demand < current_demand) {
                *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = mapped_demand;
                kbo_foreign_fa_demand_remap_remember(player_id, current_demand, mapped_demand);
                foreign_demand_mapped++;
                player_changed = 1;
                LONG foreign_detail_slot = InterlockedIncrement(&foreign_detail_log_count);
                if (foreign_detail_slot <= KBO_NO_MINOR_DEMAND_FLOOR_SCAN_MAX_DETAIL_LOGS) {
                    append_logf(
                        "KBO foreign FA demand remapped: source=%s player=%u asian_quota=%d old_demand=%d mapped_demand=%d original_superstar=%d foreign_superstar=%d",
                        source,
                        player_id,
                        asian_quota,
                        current_demand,
                        mapped_demand,
                        memory_range_readable(financials + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET, sizeof(int32_t))
                            ? *(int32_t*)(financials + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET)
                            : 0,
                        kbo_get_foreign_fa_demand_baseline_value_for_player(8, asian_quota));
                }
            }
        }
        if (player_changed) {
            changed++;
            LONG detail_slot = InterlockedIncrement(&detail_log_count);
            if (detail_slot <= KBO_NO_MINOR_DEMAND_FLOOR_SCAN_MAX_DETAIL_LOGS) {
                append_logf(
                    "KBO no-minor demand floor prescan applied: source=%s player=%u old_demand=%d floor=%d observed_contract_level=%u",
                    source,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    old_demand,
                    salary_floor,
                    (unsigned)old_contract_level);
            }
        }
    }

    HeapFree(GetProcessHeap(), 0, player_snapshot);

    static LONG summary_log_count = 0;
    LONG summary_slot = changed > 0 ? InterlockedIncrement(&summary_log_count) : 0;
    if (changed > 0 && (summary_slot <= 20 || (summary_slot % 40) == 0)) {
        append_logf(
            "KBO no-minor demand floor prescan complete: source=%s league=%u vector_off=0x%x scanned=%d teamless=%d changed=%d demand_fixed=%d foreign_demand_mapped=%d level_observed_nonmajor=%d floor=%d summary_slot=%ld",
            source,
            league_id,
            vector_offset,
            scanned,
            teamless,
            changed,
            demand_fixed,
            foreign_demand_mapped,
            level_observed_nonmajor,
            salary_floor,
            summary_slot);
    }
    return changed;
}

DWORD WINAPI kbo_no_minor_contract_demand_floor_scanner_thread(LPVOID param)
{
    (void)param;
    if (!kbo_runtime_sleep_should_continue(KBO_NO_MINOR_DEMAND_FLOOR_SCAN_INITIAL_DELAY_MS)) {
        InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
        append_log_line("stopped KBO no-minor demand floor scanner thread");
        return 0;
    }

    for (uint32_t attempt = 0; kbo_runtime_threads_should_continue(); attempt++) {
        kbo_no_minor_scan_and_floor_teamless_fa_demands("background_prescan");
        if (!kbo_runtime_sleep_should_continue(attempt < KBO_NO_MINOR_DEMAND_FLOOR_SCAN_WARMUP_ATTEMPTS
            ? KBO_NO_MINOR_DEMAND_FLOOR_SCAN_WARMUP_INTERVAL_MS
            : KBO_NO_MINOR_DEMAND_FLOOR_SCAN_INTERVAL_MS)) {
            break;
        }
    }
    InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
    append_log_line("stopped KBO no-minor demand floor scanner thread");
    return 0;
}

void start_kbo_no_minor_contract_demand_floor_scanner_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_no_minor_contract_demand_floor_scanner_thread, NULL, 0, NULL);
    if (thread == NULL) {
        append_logf("failed to start KBO no-minor demand floor scanner thread error=%lu", GetLastError());
        InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
        return;
    }
    kbo_register_runtime_thread(thread, "no-minor demand floor scanner");
    append_log_line("started KBO no-minor demand floor scanner thread");
}

