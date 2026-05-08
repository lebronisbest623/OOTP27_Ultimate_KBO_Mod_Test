/* Submit-offer probe hook. Included from native/src/foreign/foreign_signability_hooks.inc. */

typedef void (__fastcall *OotpFaSubmitOfferProbeFn)(void* screen);
typedef int (__fastcall *OotpFaOfferScreenCallbackProbeFn)(void* screen, void* sender, uintptr_t callback_id, uintptr_t value);
typedef int (__fastcall *OotpFaContractOfferCallbackProbeFn)(void* offer, void* sender, uintptr_t callback_id, uintptr_t value);
typedef uint8_t (__fastcall *OotpPlayerActionEligibilityFn)(void* action_context, int32_t action_id, uint8_t strict_check);
typedef uint8_t* (__fastcall *OotpLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);

static LONG g_kbo_no_minor_contract_demand_floor_enabled = 0;

typedef struct KboFinancialSalaryLadderSnapshot {
    uint8_t* financials;
    int32_t values[9];
    LONG active;
} KboFinancialSalaryLadderSnapshot;

typedef struct KboForeignFaDemandRemapRecord {
    uint32_t player_id;
    int32_t original_demand;
    int32_t mapped_demand;
} KboForeignFaDemandRemapRecord;

static KboFinancialSalaryLadderSnapshot g_kbo_foreign_fa_demand_ladder_snapshot = {0};
static KboForeignFaDemandRemapRecord g_kbo_foreign_fa_demand_remap_records[512] = {0};
static volatile LONG g_kbo_foreign_fa_demand_remap_record_cursor = 0;

static const uint32_t KBO_FINANCIALS_SALARY_LADDER_OFFSETS[9] = {
    0x278u, 0x280u, 0x284u, 0x288u, 0x28cu, 0x290u, 0x294u, 0x298u, 0x29cu
};

static int32_t kbo_no_minor_resolve_current_league_id(void);
static int kbo_no_minor_player_is_teamless_demand_floor_candidate(uint8_t* player, uint32_t league_id);
static void kbo_restore_foreign_fa_demand_salary_ladder(const char* source);
static uint8_t* kbo_resolve_current_league_financials(uint32_t* out_league_id);

static int kbo_foreign_fa_demand_remap_already_applied(uint32_t player_id, int32_t demand)
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

static void kbo_foreign_fa_demand_remap_remember(uint32_t player_id, int32_t original_demand, int32_t mapped_demand)
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

static int kbo_foreign_fa_demand_remap_candidate(uint8_t* player)
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

static int32_t kbo_foreign_fa_remap_demand_from_salary_ladder(int32_t demand, uint8_t* financials)
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
        foreign_values[i] = kbo_get_foreign_fa_demand_baseline_value(i);
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

static DWORD WINAPI kbo_foreign_fa_demand_restore_timer_thread(void* param)
{
    (void)param;
    Sleep(750);
    kbo_restore_foreign_fa_demand_salary_ladder("offer_build_timer");
    return 0;
}

static int kbo_write_i32(uint8_t* address, int32_t value)
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

static void kbo_restore_foreign_fa_demand_salary_ladder(const char* source)
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

static void kbo_schedule_foreign_fa_demand_restore_timer(void)
{
    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_fa_demand_restore_timer_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    }
}

__declspec(noinline) void ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper(
    uintptr_t financials_ptr,
    uintptr_t player_ptr,
    uint32_t source_rva)
{
    kbo_restore_foreign_fa_demand_salary_ladder("prepare_enter");

    if (financials_ptr == 0 || !memory_range_readable((void*)(financials_ptr + 0x29cu), sizeof(int32_t))) {
        return;
    }
    if (!kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    uint8_t* financials = (uint8_t*)financials_ptr;
    for (int i = 0; i < 9; i++) {
        g_kbo_foreign_fa_demand_ladder_snapshot.values[i] =
            *(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]);
    }
    g_kbo_foreign_fa_demand_ladder_snapshot.financials = financials;

    int patched = 0;
    for (int i = 0; i < 9; i++) {
        patched += kbo_write_i32(
            financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i],
            kbo_get_foreign_fa_demand_baseline_value(i));
    }

    if (patched == 9) {
        InterlockedExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 1);
    } else {
        kbo_restore_foreign_fa_demand_salary_ladder("prepare_failed");
    }

    static LONG prepare_log_count = 0;
    LONG slot = InterlockedIncrement(&prepare_log_count);
    if (slot <= 120) {
        append_logf(
            "KBO foreign FA demand baseline prepared source=0x%x player=%u financials=%p patched=%d original_superstar=%d foreign_superstar=%d",
            source_rva,
            memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                : 0u,
            (void*)financials,
            patched,
            g_kbo_foreign_fa_demand_ladder_snapshot.values[8],
            kbo_get_foreign_fa_demand_baseline_value(8));
    }
}

static void kbo_prepare_foreign_fa_offer_demand_baseline(uintptr_t player_ptr, const char* source)
{
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    uint32_t league_id = 0u;
    uint8_t* financials = kbo_resolve_current_league_financials(&league_id);
    if (financials == NULL) {
        static LONG no_financials_log_count = 0;
        LONG slot = InterlockedIncrement(&no_financials_log_count);
        if (slot <= 40) {
            append_logf(
                "KBO foreign FA demand baseline offer-build skipped source=%s reason=no_financials player=%u",
                source != NULL ? source : "",
                memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                    ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                    : 0u);
        }
        return;
    }

    uint32_t source_rva = 0u;
    if (source != NULL && strstr(source, "17B50B4") != NULL) {
        source_rva = OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA;
    } else if (source != NULL && strstr(source, "17A79BB") != NULL) {
        source_rva = OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA;
    }

    ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper((uintptr_t)financials, player_ptr, source_rva);
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0) {
        kbo_schedule_foreign_fa_demand_restore_timer();
        static LONG offer_prepare_log_count = 0;
        LONG slot = InterlockedIncrement(&offer_prepare_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO foreign FA demand baseline offer-build active source=%s player=%u league=%u financials=%p",
                source != NULL ? source : "",
                memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                    ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                    : 0u,
                league_id,
                (void*)financials);
        }
    }
}

static uint8_t* kbo_resolve_current_league_financials(uint32_t* out_league_id)
{
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }

    int32_t league_id = kbo_no_minor_resolve_current_league_id();
    if (league_id <= 0) {
        return NULL;
    }

    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0) {
        return NULL;
    }
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    OotpLeagueFinancialsLookupFn lookup =
        (OotpLeagueFinancialsLookupFn)((uintptr_t)exe + OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        return NULL;
    }

    uint8_t* financials = lookup((void*)global_db, league_id);
    if (financials == NULL || !memory_range_readable(financials + 0x390u, sizeof(int32_t))) {
        return NULL;
    }

    if (out_league_id != NULL) {
        *out_league_id = (uint32_t)league_id;
    }
    return financials;
}

static void kbo_log_financials_salary_baseline_probe(const char* source)
{
    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 80 && !read_kbo_localappdata_flag_file("enable_kbo_financials_salary_baseline_probe_log.txt")) {
        return;
    }

    uint32_t league_id = 0u;
    uint8_t* financials = kbo_resolve_current_league_financials(&league_id);
    if (financials == NULL) {
        if (slot <= 20) {
            append_logf("KBO financials salary baseline probe skipped source=%s reason=no_financials", source != NULL ? source : "");
        }
        return;
    }

    append_logf(
        "KBO financials salary baseline probe source=%s league=%u financials=%p off278=%d off27c=%d off280=%d off284=%d off288=%d off28c=%d off290=%d off294=%d off298=%d off29c=%d off2a0=%d off2a4=%d off2a8=%d off2ac=%d off360=%d off364=%d off368=%d off36c=%d off370=%d off374=%d off378=%d off37c=%d off380=%d off384=%d off388=%d off38c=%d off390=%d",
        source != NULL ? source : "",
        league_id,
        (void*)financials,
        *(int32_t*)(financials + 0x278u),
        *(int32_t*)(financials + 0x27cu),
        *(int32_t*)(financials + 0x280u),
        *(int32_t*)(financials + 0x284u),
        *(int32_t*)(financials + 0x288u),
        *(int32_t*)(financials + 0x28cu),
        *(int32_t*)(financials + 0x290u),
        *(int32_t*)(financials + 0x294u),
        *(int32_t*)(financials + 0x298u),
        *(int32_t*)(financials + 0x29cu),
        *(int32_t*)(financials + 0x2a0u),
        *(int32_t*)(financials + 0x2a4u),
        *(int32_t*)(financials + 0x2a8u),
        *(int32_t*)(financials + 0x2acu),
        *(int32_t*)(financials + 0x360u),
        *(int32_t*)(financials + 0x364u),
        *(int32_t*)(financials + 0x368u),
        *(int32_t*)(financials + 0x36cu),
        *(int32_t*)(financials + 0x370u),
        *(int32_t*)(financials + 0x374u),
        *(int32_t*)(financials + 0x378u),
        *(int32_t*)(financials + 0x37cu),
        *(int32_t*)(financials + 0x380u),
        *(int32_t*)(financials + 0x384u),
        *(int32_t*)(financials + 0x388u),
        *(int32_t*)(financials + 0x38cu),
        *(int32_t*)(financials + 0x390u));
}

static int32_t kbo_no_minor_resolve_current_league_id(void)
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

static int32_t kbo_no_minor_resolve_current_league_minimum_salary(void)
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

static int kbo_no_minor_clamp_player_demand_salary(uintptr_t player_ptr, uintptr_t screen_ptr, const char* source)
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
    if (old_contract_level != 1u) {
        *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET) = 1u;
        changed = 1;
    }
    if (old_demand < salary_floor) {
        *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = salary_floor;
        changed = 1;
    }

    if (changed) {
        static LONG clamp_log_count = 0;
        LONG clamp_slot = InterlockedIncrement(&clamp_log_count);
        if (clamp_slot <= 120) {
            append_logf(
                "KBO no-minor demand floor applied: source=%s screen=%p player=%u old_demand=%d floor=%d old_contract_level=%u",
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

static int kbo_no_minor_clamp_offer_screen_player_salary(uintptr_t screen_ptr, const char* source)
{
    if (screen_ptr == 0 || !memory_range_readable((void*)screen_ptr, 0xc0)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(screen_ptr + 0xb8u);
    if (player_id == 0u) {
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL) {
        return 0;
    }

    return kbo_no_minor_clamp_player_demand_salary((uintptr_t)player, screen_ptr, source);
}

#define KBO_NO_MINOR_DEMAND_FLOOR_SCAN_INITIAL_DELAY_MS 250u
#define KBO_NO_MINOR_DEMAND_FLOOR_SCAN_WARMUP_INTERVAL_MS 500u
#define KBO_NO_MINOR_DEMAND_FLOOR_SCAN_WARMUP_ATTEMPTS 40u
#define KBO_NO_MINOR_DEMAND_FLOOR_SCAN_INTERVAL_MS 2500u
#define KBO_NO_MINOR_DEMAND_FLOOR_SCAN_MAX_DETAIL_LOGS 80

static volatile LONG g_kbo_no_minor_contract_demand_floor_scanner_started = 0;

static int kbo_no_minor_player_is_teamless_demand_floor_candidate(uint8_t* player, uint32_t league_id)
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

    uint8_t old_contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    if (old_contract_level != 1u) {
        *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET) = 1u;
    }

    int32_t adjusted_demand = proposed_demand < salary_floor ? salary_floor : proposed_demand;
    if (adjusted_demand != proposed_demand || old_contract_level != 1u) {
        static LONG write_floor_log_count = 0;
        LONG slot = InterlockedIncrement(&write_floor_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO no-minor demand write floor: source=0x%x player=%u proposed=%d adjusted=%d floor=%d old_contract_level=%u",
                source_rva,
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                proposed_demand,
                adjusted_demand,
                salary_floor,
                (unsigned)old_contract_level);
        }
    }

    return adjusted_demand;
}

static int kbo_no_minor_scan_and_floor_teamless_fa_demands(const char* source)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 0, 0) == 0) {
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

    int scanned = 0;
    int teamless = 0;
    int changed = 0;
    int demand_fixed = 0;
    int foreign_demand_mapped = 0;
    int level_fixed = 0;
    static LONG detail_log_count = 0;
    static LONG foreign_detail_log_count = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
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
            *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET) = 1u;
            level_fixed++;
            player_changed = 1;
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
            int32_t mapped_demand = kbo_foreign_fa_remap_demand_from_salary_ladder(current_demand, financials);
            if (mapped_demand > 0 && mapped_demand < current_demand) {
                *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = mapped_demand;
                kbo_foreign_fa_demand_remap_remember(player_id, current_demand, mapped_demand);
                foreign_demand_mapped++;
                player_changed = 1;
                LONG foreign_detail_slot = InterlockedIncrement(&foreign_detail_log_count);
                if (foreign_detail_slot <= KBO_NO_MINOR_DEMAND_FLOOR_SCAN_MAX_DETAIL_LOGS) {
                    append_logf(
                        "KBO foreign FA demand remapped: source=%s player=%u old_demand=%d mapped_demand=%d original_superstar=%d foreign_superstar=%d",
                        source,
                        player_id,
                        current_demand,
                        mapped_demand,
                        memory_range_readable(financials + 0x29cu, sizeof(int32_t))
                            ? *(int32_t*)(financials + 0x29cu)
                            : 0,
                        kbo_get_foreign_fa_demand_baseline_value(8));
                }
            }
        }
        if (player_changed) {
            changed++;
            LONG detail_slot = InterlockedIncrement(&detail_log_count);
            if (detail_slot <= KBO_NO_MINOR_DEMAND_FLOOR_SCAN_MAX_DETAIL_LOGS) {
                append_logf(
                    "KBO no-minor demand floor prescan applied: source=%s player=%u old_demand=%d floor=%d old_contract_level=%u",
                    source,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    old_demand,
                    salary_floor,
                    (unsigned)old_contract_level);
            }
        }
    }

    if (changed > 0) {
        append_logf(
            "KBO no-minor demand floor prescan complete: source=%s league=%u vector_off=0x%x scanned=%d teamless=%d changed=%d demand_fixed=%d foreign_demand_mapped=%d level_fixed=%d floor=%d",
            source,
            league_id,
            vector_offset,
            scanned,
            teamless,
            changed,
            demand_fixed,
            foreign_demand_mapped,
            level_fixed,
            salary_floor);
    }
    return changed;
}

static DWORD WINAPI kbo_no_minor_contract_demand_floor_scanner_thread(LPVOID param)
{
    (void)param;
    Sleep(KBO_NO_MINOR_DEMAND_FLOOR_SCAN_INITIAL_DELAY_MS);

    for (uint32_t attempt = 0;; attempt++) {
        if (kbo_fix_enabled()) {
            kbo_no_minor_scan_and_floor_teamless_fa_demands("background_prescan");
        }
        Sleep(attempt < KBO_NO_MINOR_DEMAND_FLOOR_SCAN_WARMUP_ATTEMPTS
            ? KBO_NO_MINOR_DEMAND_FLOOR_SCAN_WARMUP_INTERVAL_MS
            : KBO_NO_MINOR_DEMAND_FLOOR_SCAN_INTERVAL_MS);
    }
    return 0;
}

static void start_kbo_no_minor_contract_demand_floor_scanner_thread(void)
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
    CloseHandle(thread);
    append_log_line("started KBO no-minor demand floor scanner thread");
}

static uint32_t kbo_resolve_submit_offer_screen_player_id(uintptr_t screen_ptr, uint32_t today, uint32_t* out_offset)
{
    if (out_offset != NULL) {
        *out_offset = 0xffffffffu;
    }
    if (screen_ptr == 0 || !memory_range_readable((void*)screen_ptr, 0x400)) {
        return 0;
    }

    uint32_t direct_player_id = *(uint32_t*)(screen_ptr + 0xb8u);
    uint32_t holder_team_id = 0;
    if (direct_player_id != 0u && kbo_find_active_foreign_waiver_holder(direct_player_id, today, &holder_team_id)) {
        if (out_offset != NULL) {
            *out_offset = 0xb8u;
        }
        return direct_player_id;
    }

    for (uint32_t offset = 0x20u; offset < 0x400u; offset += sizeof(uint32_t)) {
        uint32_t candidate_id = *(uint32_t*)(screen_ptr + offset);
        if (candidate_id == 0u || candidate_id > 10000000u) {
            continue;
        }
        if (kbo_find_active_foreign_waiver_holder(candidate_id, today, &holder_team_id)) {
            if (out_offset != NULL) {
                *out_offset = offset;
            }
            return candidate_id;
        }
    }
    return direct_player_id;
}

__declspec(noinline) void ootp_kbo_fa_submit_offer_probe_wrapper(
    uintptr_t screen_ptr,
    uintptr_t original_func_ptr)
{
    uint8_t* custom_policy_player = NULL;
    uint32_t custom_policy_team_id = 0u;
    uint32_t custom_policy_today = 0u;

    kbo_no_minor_clamp_offer_screen_player_salary(screen_ptr, "submit_probe");

    if (screen_ptr != 0 && memory_range_readable((void*)screen_ptr, 0xc0)) {
        uint32_t today = 0;
        uint32_t holder_team_id = 0;
        uint32_t requester_team_id = 0;
        uint32_t allowed_team_id = 0;
        uint32_t player_offset = 0xffffffffu;
        uint32_t player_id = 0;
        if (kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
            player_id = kbo_resolve_submit_offer_screen_player_id(screen_ptr, today, &player_offset);
        }
        static LONG entry_log_count = 0;
        LONG entry_slot = InterlockedIncrement(&entry_log_count);
        if (entry_slot <= 80) {
            append_logf(
                "foreign reserve submit-offer entry: screen=%p player=%u offset=0x%x today=%u cached_player=%ld cached_team=%ld cached_holder=%ld custom_cached_player=%ld custom_cached_team=%ld",
                (void*)screen_ptr,
                player_id,
                player_offset,
                today,
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_player_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_requester_team_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_holder_team_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_player_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, 0, 0));
        }
        if (player_id != 0u && kbo_military_submit_offer_should_block(screen_ptr, player_id, today)) {
            return;
        }
        if (player_id != 0u
                && kbo_recent_custom_foreign_policy_block_matches(player_id, today, &requester_team_id)) {
            static LONG custom_submit_block_log_count = 0;
            LONG block_slot = InterlockedIncrement(&custom_submit_block_log_count);
            if (block_slot <= 200) {
                append_logf(
                    "custom foreign policy submit-offer blocked: screen=%p player=%u requester_team=%u today=%u",
                    (void*)screen_ptr,
                    player_id,
                    requester_team_id,
                    today);
            }
            return;
        }
        if (player_id != 0u
                && kbo_recent_custom_foreign_policy_allow_matches(player_id, today, &allowed_team_id)) {
            requester_team_id = allowed_team_id;
            uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
            if (player != NULL
                    && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                    && kbo_custom_foreign_policy_enabled()
                    && kbo_player_is_foreign_for_kbo_rights(player)) {
                uint32_t effective_before = 0u;
                uint32_t effective_after = 0u;
                uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
                uint8_t slot_type = 0u;
                uint32_t injured_player_id = 0u;
                int allowed = kbo_custom_foreign_policy_team_allows_candidate(
                    allowed_team_id,
                    player,
                    &effective_before,
                    &effective_after,
                    &effective_limit,
                    &slot_type,
                    &injured_player_id);
                if (!allowed) {
                    kbo_record_recent_custom_foreign_policy_block(player_id, allowed_team_id, today);
                    static LONG custom_submit_slot_block_log_count = 0;
                    LONG block_slot = InterlockedIncrement(&custom_submit_slot_block_log_count);
                    if (block_slot <= 200) {
                        append_logf(
                            "custom foreign policy submit-offer blocked by pending count: screen=%p player=%u requester_team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                            (void*)screen_ptr,
                            player_id,
                            allowed_team_id,
                            effective_before,
                            effective_after,
                            effective_limit,
                            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                            injured_player_id,
                            today);
                    }
                    return;
                }
                custom_policy_player = player;
                custom_policy_team_id = allowed_team_id;
                custom_policy_today = today;
            }
        }
        if (player_id != 0u
            && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)) {
            static LONG probe_log_count = 0;
            LONG slot = InterlockedIncrement(&probe_log_count);
            if (slot <= 200) {
                append_logf(
                    "foreign reserve submit-offer probe: screen=%p player=%u holder_team=%u today=%u",
                    (void*)screen_ptr,
                    player_id,
                    holder_team_id,
                    today);
            }
            int recent_block = kbo_recent_foreign_offer_block_matches(player_id, today, &requester_team_id, &holder_team_id);
            int recent_allow = kbo_recent_foreign_offer_allow_matches(player_id, today, &allowed_team_id);
            if ((!recent_block || requester_team_id != holder_team_id)
                    && (!recent_allow || allowed_team_id != holder_team_id)) {
                static LONG submit_block_log_count = 0;
                LONG block_slot = InterlockedIncrement(&submit_block_log_count);
                if (block_slot <= 200) {
                    append_logf(
                        "foreign reserve submit-offer blocked: screen=%p player=%u requester_team=%u holder_team=%u today=%u reason=%s",
                        (void*)screen_ptr,
                        player_id,
                        requester_team_id,
                        holder_team_id,
                        today,
                        recent_block ? "recent_block" : "active_right");
                }
                return;
            }
            if (custom_policy_team_id == 0u && recent_allow && allowed_team_id == holder_team_id) {
                uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
                if (player != NULL
                        && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                        && kbo_custom_foreign_policy_enabled()
                        && kbo_player_is_foreign_for_kbo_rights(player)) {
                    uint32_t effective_before = 0u;
                    uint32_t effective_after = 0u;
                    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
                    uint8_t slot_type = 0u;
                    uint32_t injured_player_id = 0u;
                    int allowed = kbo_custom_foreign_policy_team_allows_candidate(
                        holder_team_id,
                        player,
                        &effective_before,
                        &effective_after,
                        &effective_limit,
                        &slot_type,
                        &injured_player_id);
                    if (!allowed) {
                        kbo_record_recent_custom_foreign_policy_block(player_id, holder_team_id, today);
                        static LONG holder_submit_slot_block_log_count = 0;
                        LONG block_slot = InterlockedIncrement(&holder_submit_slot_block_log_count);
                        if (block_slot <= 200) {
                            append_logf(
                                "custom foreign policy holder submit-offer blocked by pending count: screen=%p player=%u requester_team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                                (void*)screen_ptr,
                                player_id,
                                holder_team_id,
                                effective_before,
                                effective_after,
                                effective_limit,
                                slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                                injured_player_id,
                                today);
                        }
                        return;
                    }
                    custom_policy_player = player;
                    custom_policy_team_id = holder_team_id;
                    custom_policy_today = today;
                }
            }
        }
    }

    OotpFaSubmitOfferProbeFn original_func = (OotpFaSubmitOfferProbeFn)original_func_ptr;
    if (original_func != NULL) {
        original_func((void*)screen_ptr);
    }
    if (custom_policy_player != NULL && custom_policy_team_id != 0u && custom_policy_today != 0u) {
        kbo_record_custom_foreign_pending_offer(custom_policy_team_id, custom_policy_player, custom_policy_today);
    }
}

__declspec(noinline) void ootp_kbo_fa_offer_player_demand_floor_probe(
    uintptr_t player_ptr,
    uintptr_t screen_ptr,
    uint32_t source_rva)
{
    const char* source = "offer_build";
    if (source_rva == OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA) {
        source = "offer_build_17A79BB";
    } else if (source_rva == OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA) {
        source = "offer_build_17B50B4";
    }
    kbo_no_minor_clamp_player_demand_salary(player_ptr, screen_ptr, source);
}

__declspec(noinline) int ootp_kbo_fa_offer_screen_callback_probe_wrapper(
    uintptr_t screen_ptr,
    uintptr_t sender_ptr,
    uintptr_t callback_id,
    uintptr_t value,
    uintptr_t original_func_ptr)
{
    int result = 0;
    uint32_t player_id = 0;
    int32_t selected_mode = -1;
    uintptr_t contract_type_control = 0;
    uintptr_t major_option_control = 0;
    uintptr_t submit_control = 0;

    if (screen_ptr != 0 && memory_range_readable((void*)screen_ptr, 0x310)) {
        player_id = *(uint32_t*)(screen_ptr + 0xb8u);
        contract_type_control = *(uintptr_t*)(screen_ptr + 0x2f8u);
        major_option_control = *(uintptr_t*)(screen_ptr + 0x2f0u);
        submit_control = *(uintptr_t*)(screen_ptr + 0x2e0u);
        uintptr_t owner = *(uintptr_t*)(screen_ptr + 0xc8u);
        if (owner != 0 && memory_range_readable((void*)owner, 0x108)) {
            selected_mode = *(int32_t*)(owner + 0x104u);
        }
    }

    static LONG callback_log_count = 0;
    LONG slot = InterlockedIncrement(&callback_log_count);
    if (slot <= 400 && callback_id >= 0xf60u && callback_id <= 0xfa0u) {
        append_logf(
            "KBO no-minor offer callback: screen=%p sender=%p id=0x%llx value=%p player=%u selected=%d contract_ctl=%p major_option_ctl=%p submit_ctl=%p",
            (void*)screen_ptr,
            (void*)sender_ptr,
            (unsigned long long)callback_id,
            (void*)value,
            player_id,
            selected_mode,
            (void*)contract_type_control,
            (void*)major_option_control,
            (void*)submit_control);
    }

    OotpFaOfferScreenCallbackProbeFn original_func = (OotpFaOfferScreenCallbackProbeFn)original_func_ptr;
    if (original_func != NULL) {
        result = original_func((void*)screen_ptr, (void*)sender_ptr, callback_id, value);
    }

    if (screen_ptr != 0 && memory_range_readable((void*)screen_ptr, 0x310)) {
        uintptr_t owner = *(uintptr_t*)(screen_ptr + 0xc8u);
        int32_t after_mode = -1;
        if (owner != 0 && memory_range_readable((void*)owner, 0x108)) {
            after_mode = *(int32_t*)(owner + 0x104u);
        }
        LONG after_slot = InterlockedIncrement(&callback_log_count);
        if (after_slot <= 400 && callback_id >= 0xf60u && callback_id <= 0xfa0u) {
            append_logf(
                "KBO no-minor offer callback after: screen=%p id=0x%llx result=%d selected=%d",
                (void*)screen_ptr,
                (unsigned long long)callback_id,
                result,
                after_mode);
        }
    }

    return result;
}

__declspec(noinline) int ootp_kbo_fa_contract_offer_callback_probe_wrapper(
    uintptr_t offer_ptr,
    uintptr_t sender_ptr,
    uintptr_t callback_id,
    uintptr_t value,
    uintptr_t original_func_ptr)
{
    int result = 0;
    int32_t selected_player = -1;
    int32_t before_salary = -1;
    int32_t before_option_salary = -1;
    uint8_t before_major_flag = 0xffu;

    if (offer_ptr != 0 && memory_range_readable((void*)offer_ptr, 0xd0)) {
        selected_player = *(int32_t*)(offer_ptr + 0xbcu);
        before_salary = *(int32_t*)(offer_ptr + 0xc0u);
        before_option_salary = *(int32_t*)(offer_ptr + 0xc8u);
        before_major_flag = *(uint8_t*)(offer_ptr + 0xccu);
    }

    static LONG contract_callback_log_count = 0;
    LONG slot = InterlockedIncrement(&contract_callback_log_count);
    if (slot <= 600) {
        append_logf(
            "KBO no-minor contract callback: offer=%p sender=%p id=0x%llx value=%p player=%d salary=%d option_salary=%d major_flag=%u",
            (void*)offer_ptr,
            (void*)sender_ptr,
            (unsigned long long)callback_id,
            (void*)value,
            selected_player,
            before_salary,
            before_option_salary,
            (unsigned)before_major_flag);
    }

    OotpFaContractOfferCallbackProbeFn original_func = (OotpFaContractOfferCallbackProbeFn)original_func_ptr;
    if (original_func != NULL) {
        result = original_func((void*)offer_ptr, (void*)sender_ptr, callback_id, value);
    }

    if (offer_ptr != 0 && memory_range_readable((void*)offer_ptr, 0xd0)) {
        int32_t after_salary = *(int32_t*)(offer_ptr + 0xc0u);
        int32_t after_option_salary = *(int32_t*)(offer_ptr + 0xc8u);
        uint8_t after_major_flag = *(uint8_t*)(offer_ptr + 0xccu);
        *(uint8_t*)(offer_ptr + 0xccu) = 1u;

        LONG after_slot = InterlockedIncrement(&contract_callback_log_count);
        if (after_slot <= 600) {
            append_logf(
                "KBO no-minor contract callback after: offer=%p id=0x%llx result=%d salary=%d option_salary=%d major_flag=%u forced_major_flag=1",
                (void*)offer_ptr,
                (unsigned long long)callback_id,
                result,
                after_salary,
                after_option_salary,
                (unsigned)after_major_flag);
        }
    }

    return result;
}

__declspec(noinline) uint8_t ootp_kbo_player_action_eligibility_wrapper(
    uintptr_t action_context,
    int32_t action_id,
    uint8_t strict_check,
    uintptr_t original_func_ptr)
{
    if (kbo_military_player_action_should_block(action_context, action_id, strict_check)) {
        return 0;
    }

    /* OOTP player action ids 0x35/0x36 are Offer Minor Lg Contract/Extension. */
    if (action_id == 0x35 || action_id == 0x36) {
        static LONG blocked_log_count = 0;
        LONG slot = InterlockedIncrement(&blocked_log_count);
        if (slot <= 80) {
            append_logf(
                "KBO no-minor player action blocked: context=%p action=0x%x strict=%u",
                (void*)action_context,
                action_id,
                (unsigned)strict_check);
        }
        return 0;
    }

    OotpPlayerActionEligibilityFn original_func = (OotpPlayerActionEligibilityFn)original_func_ptr;
    if (original_func == NULL) {
        return 0;
    }
    return original_func((void*)action_context, action_id, strict_check);
}
