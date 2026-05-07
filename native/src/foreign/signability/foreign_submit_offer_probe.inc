/* Submit-offer probe hook. Included from native/src/foreign/foreign_signability_hooks.inc. */

typedef void (__fastcall *OotpFaSubmitOfferProbeFn)(void* screen);
typedef int (__fastcall *OotpFaOfferScreenCallbackProbeFn)(void* screen, void* sender, uintptr_t callback_id, uintptr_t value);
typedef int (__fastcall *OotpFaContractOfferCallbackProbeFn)(void* offer, void* sender, uintptr_t callback_id, uintptr_t value);
typedef uint8_t (__fastcall *OotpPlayerActionEligibilityFn)(void* action_context, int32_t action_id, uint8_t strict_check);
typedef uint8_t* (__fastcall *OotpLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);

static LONG g_kbo_no_minor_contract_demand_floor_enabled = 0;

static int kbo_no_minor_player_is_teamless_demand_floor_candidate(uint8_t* player, uint32_t league_id);

static int32_t kbo_no_minor_resolve_current_league_id(void)
{
    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0
            || !memory_range_readable(
                (void*)(global_db + OOTP27_GLOBAL_CURRENT_LEAGUE_OFFSET),
                sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t current_league_context = *(uintptr_t*)(global_db + OOTP27_GLOBAL_CURRENT_LEAGUE_OFFSET);
    if (current_league_context == 0
            || !memory_range_readable(
                (void*)(current_league_context + OOTP27_GLOBAL_CURRENT_LEAGUE_ID_OFFSET),
                sizeof(int32_t))) {
        return 0;
    }

    int32_t league_id = *(int32_t*)(current_league_context + OOTP27_GLOBAL_CURRENT_LEAGUE_ID_OFFSET);
    if (league_id <= 0 || league_id > 100000) {
        return 0;
    }
    return league_id;
}

static int32_t kbo_no_minor_resolve_current_league_minimum_salary(void)
{
    int32_t league_id = kbo_no_minor_resolve_current_league_id();
    if (league_id <= 0) {
        return 0;
    }

    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0) {
        return 0;
    }
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return 0;
    }

    OotpLeagueFinancialsLookupFn lookup =
        (OotpLeagueFinancialsLookupFn)((uintptr_t)exe + OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        return 0;
    }

    uint8_t* financials = lookup((void*)global_db, league_id);
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
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 0, 0) == 0) {
        return proposed_demand;
    }
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
    int level_fixed = 0;
    static LONG detail_log_count = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;

        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_no_minor_player_is_teamless_demand_floor_candidate(player, league_id)) {
            continue;
        }
        teamless++;

        if (!memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))
                || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET, sizeof(uint8_t))) {
            continue;
        }

        int32_t old_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
        uint8_t old_contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
        int player_changed = 0;
        if (old_contract_level != 1u) {
            *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET) = 1u;
            level_fixed++;
            player_changed = 1;
        }
        if (old_demand < salary_floor) {
            *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = salary_floor;
            demand_fixed++;
            player_changed = 1;
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
            "KBO no-minor demand floor prescan complete: source=%s league=%u vector_off=0x%x scanned=%d teamless=%d changed=%d demand_fixed=%d level_fixed=%d floor=%d",
            source,
            league_id,
            vector_offset,
            scanned,
            teamless,
            changed,
            demand_fixed,
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
