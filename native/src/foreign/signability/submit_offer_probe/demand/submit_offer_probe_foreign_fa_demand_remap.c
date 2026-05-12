#include "../internal/submit_offer_probe_internal.h"

int kbo_foreign_fa_demand_remap_already_applied(uint32_t player_id, int32_t demand)
{
    if (player_id == 0u || demand <= 0) {
        return 0;
    }
    for (int i = 0; i < (int)(sizeof(g_kbo_foreign_fa_demand_remap_records) / sizeof(g_kbo_foreign_fa_demand_remap_records[0])); i++) {
        if (g_kbo_foreign_fa_demand_remap_records[i].player_id == player_id
                && g_kbo_foreign_fa_demand_remap_records[i].mapped_demand == demand) {
            return 1;
        }
    }
    return 0;
}

void kbo_foreign_fa_demand_remap_remember(uint32_t player_id, int32_t original_demand, int32_t mapped_demand)
{
    if (player_id == 0u || original_demand <= 0 || mapped_demand <= 0) {
        return;
    }
    for (int i = 0; i < (int)(sizeof(g_kbo_foreign_fa_demand_remap_records) / sizeof(g_kbo_foreign_fa_demand_remap_records[0])); i++) {
        if (g_kbo_foreign_fa_demand_remap_records[i].player_id == player_id) {
            g_kbo_foreign_fa_demand_remap_records[i].original_demand = original_demand;
            g_kbo_foreign_fa_demand_remap_records[i].mapped_demand = mapped_demand;
            return;
        }
    }

    LONG slot = InterlockedIncrement((volatile LONG*)&g_kbo_foreign_fa_demand_remap_record_cursor);
    int index = (int)((slot - 1) % (LONG)(sizeof(g_kbo_foreign_fa_demand_remap_records) / sizeof(g_kbo_foreign_fa_demand_remap_records[0])));
    g_kbo_foreign_fa_demand_remap_records[index].player_id = player_id;
    g_kbo_foreign_fa_demand_remap_records[index].original_demand = original_demand;
    g_kbo_foreign_fa_demand_remap_records[index].mapped_demand = mapped_demand;
}

int kbo_foreign_fa_demand_remap_candidate(uint8_t* player)
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
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }
    if (!memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))) {
        return 0;
    }
    int32_t demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    return demand > 0 && demand < 1000000000;
}

int32_t kbo_foreign_fa_remap_demand_from_salary_ladder(int32_t demand, uint8_t* financials, int asian_quota)
{
    if (demand <= 0 || financials == NULL) {
        return demand;
    }

    int32_t original[9];
    int32_t foreign_values[9];
    for (int i = 0; i < 9; i++) {
        if (!memory_range_readable(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i], sizeof(int32_t))) {
            return demand;
        }
        original[i] = *(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]);
        foreign_values[i] = kbo_get_foreign_fa_demand_baseline_value_for_player(i, asian_quota);
    }

    if (demand <= original[0]) {
        return demand < foreign_values[0] ? demand : foreign_values[0];
    }

    for (int i = 1; i < 9; i++) {
        if (original[i] <= original[i - 1]) {
            continue;
        }
        if (demand <= original[i]) {
            int64_t src_span = (int64_t)original[i] - (int64_t)original[i - 1];
            int64_t dst_span = (int64_t)foreign_values[i] - (int64_t)foreign_values[i - 1];
            int64_t src_delta = (int64_t)demand - (int64_t)original[i - 1];
            int64_t mapped = (int64_t)foreign_values[i - 1] + ((src_delta * dst_span) / src_span);
            if (mapped < foreign_values[0]) {
                mapped = foreign_values[0];
            }
            if (mapped > INT32_MAX) {
                mapped = INT32_MAX;
            }
            return (int32_t)mapped;
        }
    }

    if (original[8] <= 0 || foreign_values[8] <= 0) {
        return demand;
    }
    int64_t mapped = ((int64_t)demand * (int64_t)foreign_values[8]) / (int64_t)original[8];
    if (mapped < foreign_values[8]) {
        mapped = foreign_values[8];
    }
    if (mapped > INT32_MAX) {
        mapped = INT32_MAX;
    }
    return (int32_t)mapped;
}

DWORD WINAPI kbo_foreign_fa_demand_restore_timer_thread(void* param)
{
    (void)param;
    if (!kbo_runtime_sleep_should_continue(750)) {
        return 0;
    }
    kbo_restore_foreign_fa_demand_salary_ladder("offer_build_timer");
    return 0;
}

int kbo_write_i32(uint8_t* address, int32_t value)
{
    if (address == NULL || !memory_range_readable(address, sizeof(int32_t))) {
        return 0;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(address, sizeof(int32_t), PAGE_READWRITE, &old_protect)) {
        return 0;
    }

    *(int32_t*)address = value;
    DWORD ignored = 0;
    VirtualProtect(address, sizeof(int32_t), old_protect, &ignored);
    return 1;
}

void kbo_restore_foreign_fa_demand_salary_ladder(const char* source)
{
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) == 0) {
        return;
    }
    if (InterlockedExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0) == 0) {
        return;
    }

    uint8_t* financials = g_kbo_foreign_fa_demand_ladder_snapshot.financials;
    int restored = 0;
    if (financials != NULL) {
        for (int i = 0; i < 9; i++) {
            restored += kbo_write_i32(
                financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i],
                g_kbo_foreign_fa_demand_ladder_snapshot.values[i]);
        }
    }

    static LONG restore_log_count = 0;
    LONG slot = InterlockedIncrement(&restore_log_count);
    if (slot <= 80) {
        append_logf(
            "KBO foreign FA demand baseline restored source=%s financials=%p restored=%d",
            source != NULL ? source : "",
            (void*)financials,
            restored);
    }

    g_kbo_foreign_fa_demand_ladder_snapshot.financials = NULL;
}

void kbo_schedule_foreign_fa_demand_restore_timer(void)
{
    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_fa_demand_restore_timer_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "foreign FA demand restore timer");
    }
}

