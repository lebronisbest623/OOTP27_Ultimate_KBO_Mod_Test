/* Foreign-player offer/signability hooks. Included from native/src/foreign_waiver_ai.inc. */

static void kbo_record_recent_foreign_offer_block(uint32_t player_id, uint32_t requester_team_id, uint32_t holder_team_id, uint32_t today);
static int kbo_recent_foreign_offer_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id, uint32_t* out_holder_team_id);
static void kbo_log_asian_quota_offer_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    uint8_t original_result,
    int32_t flag);
static void kbo_log_asian_quota_signability_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    int original_signability,
    uintptr_t caller_rva);

static void kbo_log_foreign_signability_block_callsite(
    uintptr_t caller_rva,
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today)
{
    enum { KBO_SIGNABILITY_CALLSITE_LOG_SLOTS = 64 };
    static volatile LONG logged_count = 0;
    static uintptr_t logged_callers[KBO_SIGNABILITY_CALLSITE_LOG_SLOTS] = {0};

    if (caller_rva == 0) {
        return;
    }
    for (int i = 0; i < KBO_SIGNABILITY_CALLSITE_LOG_SLOTS; i++) {
        if (logged_callers[i] == caller_rva) {
            return;
        }
    }

    LONG slot = InterlockedIncrement(&logged_count) - 1;
    if (slot < 0 || slot >= KBO_SIGNABILITY_CALLSITE_LOG_SLOTS) {
        return;
    }
    logged_callers[slot] = caller_rva;
    append_logf(
        "foreign reserve signability: new block callsite caller_rva=0x%llx player=%u requester_team=%u holder_team=%u original=%d today=%u",
        (unsigned long long)caller_rva,
        player_id,
        requester_team_id,
        holder_team_id,
        original_signability,
        today);
}

static int kbo_team_id_is_military_service_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    static uint32_t cached_sang_id = 0u;
    static uint32_t cached_kpb_id = 0u;
    if (cached_sang_id == 0u) {
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        if (sang != NULL && memory_range_readable(sang + OOTP27_KBO_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            cached_sang_id = *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET);
        }
    }
    if (cached_kpb_id == 0u) {
        uint8_t* kpb = find_kbo_team_by_csv_id_any_league("KPB", 0);
        if (kpb != NULL && memory_range_readable(kpb + OOTP27_KBO_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            cached_kpb_id = *(uint32_t*)(kpb + OOTP27_KBO_TEAM_ID_OFFSET);
        }
    }

    return team_id == cached_sang_id || (cached_kpb_id != 0u && team_id == cached_kpb_id);
}

static int kbo_fast_block_fa_candidate_before_original(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    const char* context,
    uint32_t* out_player_id)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (!kbo_fix_enabled() || requesting_team_id <= 0) {
        return 0;
    }

    uint32_t requester_team_id = (uint32_t)requesting_team_id;
    if (kbo_team_id_is_military_service_team(requester_team_id)) {
        uint32_t player_id = 0u;
        if (player_ptr != 0
                && memory_range_readable((void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
            player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
        }
        if (out_player_id != NULL) {
            *out_player_id = player_id;
        }
        static volatile LONG military_fast_block_log_count = 0;
        LONG slot = InterlockedIncrement(&military_fast_block_log_count);
        if (slot <= 120) {
            append_logf(
                "FA fast-block source=%s reason=military_service_team player=%u requester_team=%u",
                context != NULL ? context : "",
                player_id,
                requester_team_id);
        }
        return 1;
    }

    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }
    if (player_id == 0u) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return 0;
    }

    if (kbo_foreign_waiver_ai_enabled()) {
        static LONG load_attempted = 0;
        if (InterlockedCompareExchange(&load_attempted, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
        kbo_prune_expired_foreign_waiver_rights(today);

        uint32_t holder_team_id = 0u;
        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
                && holder_team_id != 0u
                && holder_team_id != requester_team_id) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);
            kbo_record_recent_foreign_offer_block(player_id, requester_team_id, holder_team_id, today);

            static volatile LONG rights_fast_block_log_count = 0;
            LONG slot = InterlockedIncrement(&rights_fast_block_log_count);
            if (slot <= 200) {
                append_logf(
                    "FA fast-block source=%s reason=foreign_reserve player=%u requester_team=%u holder_team=%u today=%u",
                    context != NULL ? context : "",
                    player_id,
                    requester_team_id,
                    holder_team_id,
                    today);
            }
            return 1;
        }
    }

    if (kbo_custom_foreign_policy_enabled() && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            requester_team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, requester_team_id, today);
            static volatile LONG custom_fast_block_log_count = 0;
            LONG slot = InterlockedIncrement(&custom_fast_block_log_count);
            if (slot <= 200) {
                append_logf(
                    "FA fast-block source=%s reason=custom_foreign_policy player=%u requester_team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    context != NULL ? context : "",
                    player_id,
                    requester_team_id,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    today);
            }
            return 1;
        }
    }

    return 0;
}

static int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability,
    uintptr_t caller_rva)
{
    (void)year_hint;
    if (!kbo_fix_enabled()) {
        return original_signability;
    }
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return original_signability;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        return original_signability;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return original_signability;
    }

    if (requesting_team_id > 0 && kbo_team_id_is_military_service_team((uint32_t)requesting_team_id)) {
        static volatile LONG military_team_signability_log_count = 0;
        LONG slot = InterlockedIncrement(&military_team_signability_log_count);
        if (slot <= 200) {
            append_logf(
                "military service team FA signability blocked player=%u requester_team=%d original=%d caller_rva=0x%llx",
                player_id,
                requesting_team_id,
                original_signability,
                (unsigned long long)caller_rva);
        }
        return 0; /* OOTP signability enum: 0 = Impossible. */
    }

    kbo_log_asian_quota_signability_probe(player, player_id, requesting_team_id, original_signability, caller_rva);

    uint32_t today = 0;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_signability;
    }

    uint32_t holder_team_id = 0;
    if (kbo_foreign_waiver_ai_enabled()) {
        static LONG load_attempted = 0;
        if (InterlockedCompareExchange(&load_attempted, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
        kbo_prune_expired_foreign_waiver_rights(today);

        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);

            if (requesting_team_id <= 0) {
                static LONG generic_block_log_count = 0;
                LONG slot = InterlockedIncrement(&generic_block_log_count);
                if (slot <= 200) {
                    append_logf(
                        "foreign reserve signability: blocked generic request player=%u holder_team=%u original=%d forced=0 today=%u caller_rva=0x%llx",
                        player_id,
                        holder_team_id,
                        original_signability,
                        today,
                        (unsigned long long)caller_rva);
                }
                return 0; /* OOTP signability enum: 0 = Impossible. */
            }

            uint32_t team_id = (uint32_t)requesting_team_id;
            if (team_id == holder_team_id) {
                static LONG allow_log_count = 0;
                LONG slot = InterlockedIncrement(&allow_log_count);
                if (slot <= 40) {
                    append_logf(
                        "foreign reserve signability: holder allowed player=%u team=%u original=%d today=%u caller_rva=0x%llx",
                        player_id,
                        team_id,
                        original_signability,
                        today,
                        (unsigned long long)caller_rva);
                }
                return original_signability;
            }

            static LONG block_log_count = 0;
            LONG slot = InterlockedIncrement(&block_log_count);
            if (slot <= 200) {
                append_logf(
                    "foreign reserve signability: blocked player=%u requester_team=%u holder_team=%u original=%d forced=0 today=%u caller_rva=0x%llx",
                    player_id,
                    team_id,
                    holder_team_id,
                    original_signability,
                    today,
                    (unsigned long long)caller_rva);
            }
            kbo_record_recent_foreign_offer_block(player_id, team_id, holder_team_id, today);
            kbo_log_foreign_signability_block_callsite(caller_rva, player_id, team_id, holder_team_id, original_signability, today);
            return 0; /* OOTP signability enum: 0 = Impossible. */
        }
    }

    if (requesting_team_id > 0
            && kbo_custom_foreign_policy_enabled()
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            (uint32_t)requesting_team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        int override_original_block = kbo_custom_foreign_policy_can_override_original_block(
            player,
            (uint32_t)requesting_team_id);
        int adjusted = allowed ? original_signability : 0;
        if (allowed && original_signability == 0 && override_original_block) {
            adjusted = 1;
        }

        static volatile LONG custom_policy_signability_log_count = 0;
        LONG slot = InterlockedIncrement(&custom_policy_signability_log_count);
        if (slot <= 200) {
            append_logf(
                "custom foreign policy signability player=%u requester_team=%d original=%d adjusted=%d allowed=%d override=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u caller_rva=0x%llx",
                player_id,
                requesting_team_id,
                original_signability,
                adjusted,
                allowed,
                override_original_block,
                effective_before,
                effective_after,
                effective_limit,
                slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                injured_player_id,
                today,
                (unsigned long long)caller_rva);
        }
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, (uint32_t)requesting_team_id, today);
        }
        return adjusted;
    }

    if (requesting_team_id > 0) {
        uint8_t injury_slot_type = 0u;
        uint32_t injured_player_id = 0u;
        uint32_t effective_count = 0u;
        uint32_t effective_limit = 0u;
        if (kbo_foreign_injury_replacement_signing_exception_available(
                (uint32_t)requesting_team_id,
                player,
                &injury_slot_type,
                &injured_player_id,
                &effective_count,
                &effective_limit)) {
            static LONG injury_signability_log_count = 0;
            LONG slot = InterlockedIncrement(&injury_signability_log_count);
            int adjusted = original_signability != 0 ? original_signability : 1;
            if (slot <= 120) {
                append_logf(
                    "foreign injury replacement signability allowed player=%u requester_team=%d injured=%u slot=%s original=%d adjusted=%d effective=%u limit=%u today=%u caller_rva=0x%llx",
                    player_id,
                    requesting_team_id,
                    injured_player_id,
                    kbo_foreign_injury_slot_label(injury_slot_type),
                    original_signability,
                    adjusted,
                    effective_count,
                    effective_limit,
                    today,
                    (unsigned long long)caller_rva);
            }
            return adjusted;
        }
    }

    return original_signability;
}

typedef int (__fastcall *OotpPlayerTeamSignabilityFn)(void* player, int32_t team_id, uint16_t year_hint);
typedef void (__fastcall *OotpFaSubmitOfferProbeFn)(void* screen);

static volatile LONG g_kbo_foreign_offer_block_player_id = 0;
static volatile LONG g_kbo_foreign_offer_block_requester_team_id = 0;
static volatile LONG g_kbo_foreign_offer_block_holder_team_id = 0;
static volatile LONG g_kbo_foreign_offer_block_date = 0;
static volatile LONG64 g_kbo_foreign_offer_block_tick = 0;
static volatile LONG g_kbo_custom_foreign_policy_block_player_id = 0;
static volatile LONG g_kbo_custom_foreign_policy_block_requester_team_id = 0;
static volatile LONG g_kbo_custom_foreign_policy_block_date = 0;
static volatile LONG64 g_kbo_custom_foreign_policy_block_tick = 0;

static void kbo_record_recent_foreign_offer_block(uint32_t player_id, uint32_t requester_team_id, uint32_t holder_team_id, uint32_t today)
{
    InterlockedExchange(&g_kbo_foreign_offer_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_holder_team_id, (LONG)holder_team_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_foreign_offer_block_tick, (LONG64)GetTickCount64());
}

static int kbo_recent_foreign_offer_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id, uint32_t* out_holder_team_id)
{
    if (player_id == 0u || today == 0u) {
        return 0;
    }

    LONG cached_player = InterlockedCompareExchange(&g_kbo_foreign_offer_block_player_id, 0, 0);
    LONG cached_date = InterlockedCompareExchange(&g_kbo_foreign_offer_block_date, 0, 0);
    LONG64 cached_tick = InterlockedCompareExchange64(&g_kbo_foreign_offer_block_tick, 0, 0);
    if ((uint32_t)cached_player != player_id || (uint32_t)cached_date != today) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (cached_tick <= 0 || now < (ULONGLONG)cached_tick || now - (ULONGLONG)cached_tick > 120000ull) {
        return 0;
    }

    LONG requester = InterlockedCompareExchange(&g_kbo_foreign_offer_block_requester_team_id, 0, 0);
    LONG holder = InterlockedCompareExchange(&g_kbo_foreign_offer_block_holder_team_id, 0, 0);
    if (requester <= 0 || holder <= 0 || requester == holder) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = (uint32_t)requester;
    }
    if (out_holder_team_id != NULL) {
        *out_holder_team_id = (uint32_t)holder;
    }
    return 1;
}

static void kbo_record_recent_custom_foreign_policy_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today)
{
    if (player_id == 0u || requester_team_id == 0u || today == 0u) {
        return;
    }

    InterlockedExchange(&g_kbo_custom_foreign_policy_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_custom_foreign_policy_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_custom_foreign_policy_block_tick, (LONG64)GetTickCount64());
}

static int kbo_recent_custom_foreign_policy_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id)
{
    if (player_id == 0u || today == 0u) {
        return 0;
    }

    LONG cached_player = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_player_id, 0, 0);
    LONG cached_date = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_date, 0, 0);
    LONG64 cached_tick = InterlockedCompareExchange64(&g_kbo_custom_foreign_policy_block_tick, 0, 0);
    if ((uint32_t)cached_player != player_id || (uint32_t)cached_date != today) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (cached_tick <= 0 || now < (ULONGLONG)cached_tick || now - (ULONGLONG)cached_tick > 120000ull) {
        return 0;
    }

    LONG requester = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, 0, 0);
    if (requester <= 0) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = (uint32_t)requester;
    }
    return 1;
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

__declspec(noinline) int ootp_kbo_player_team_signability_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uint16_t year_hint,
    uintptr_t original_func_ptr)
{
    if (kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "signability", NULL)) {
        return 0; /* OOTP signability enum: 0 = Impossible. */
    }

    OotpPlayerTeamSignabilityFn original_func = (OotpPlayerTeamSignabilityFn)original_func_ptr;
    int original_signability = 0;
    if (original_func != NULL) {
        original_signability = original_func((void*)player_ptr, team_id, year_hint);
    }
    uintptr_t caller_rva = 0;
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    if (exe != NULL && caller > (uintptr_t)exe) {
        caller_rva = caller - (uintptr_t)exe;
    }
    return kbo_enforce_foreign_waiver_signability(player_ptr, team_id, year_hint, original_signability, caller_rva);
}

__declspec(noinline) uint8_t ootp_kbo_player_offer_eligibility_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t flag,
    uintptr_t original_func_ptr)
{
    if (kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "offer_eligibility", NULL)) {
        return 0;
    }

    typedef uint8_t (__fastcall *OriginalOfferEligibilityFn)(void*, int32_t, int32_t);
    OriginalOfferEligibilityFn original_func = (OriginalOfferEligibilityFn)original_func_ptr;

    uint8_t original_result = 0;
    if (original_func != NULL) {
        original_result = original_func((void*)player_ptr, team_id, flag);
    }

    if (player_ptr == 0 || team_id <= 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        return original_result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    kbo_log_asian_quota_offer_probe(player, player_id, team_id, original_result, flag);

    if (kbo_team_id_is_military_service_team((uint32_t)team_id)) {
        static volatile LONG military_team_offer_log_count = 0;
        LONG slot = InterlockedIncrement(&military_team_offer_log_count);
        if (slot <= 120) {
            append_logf(
                "military service team FA offer eligibility blocked player=%u requester_team=%d original=%u flag=%d",
                player_id,
                team_id,
                (uint32_t)original_result,
                flag);
        }
        return 0;
    }

    uint32_t today = 0u;
    uint32_t holder_team_id = 0u;
    if (player_id == 0u || !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_result;
    }

    if (kbo_foreign_waiver_ai_enabled()
            && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            && holder_team_id != 0u) {
        if (holder_team_id == (uint32_t)team_id) {
            return original_result;
        }

        kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);

        static volatile LONG log_count = 0;
        LONG slot = InterlockedIncrement(&log_count);
        if (slot <= 80) {
            append_logf(
                "foreign reserve offer eligibility blocked player=%u requester_team=%d holder_team=%u original=%u flag=%d today=%u",
                player_id,
                team_id,
                holder_team_id,
                (uint32_t)original_result,
                flag,
                today);
        }

        kbo_record_recent_foreign_offer_block(player_id, (uint32_t)team_id, holder_team_id, today);
        return 0;
    }

    if (kbo_custom_foreign_policy_enabled() && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            (uint32_t)team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        int override_original_block = kbo_custom_foreign_policy_can_override_original_block(player, (uint32_t)team_id);
        uint8_t adjusted = allowed ? original_result : 0u;
        if (allowed && original_result == 0u && override_original_block) {
            adjusted = 1u;
        }

        static volatile LONG custom_policy_offer_log_count = 0;
        LONG slot = InterlockedIncrement(&custom_policy_offer_log_count);
        if (slot <= 200) {
            append_logf(
                "custom foreign policy offer player=%u requester_team=%d original=%u adjusted=%u allowed=%d override=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u flag=%d today=%u",
                player_id,
                team_id,
                (uint32_t)original_result,
                (uint32_t)adjusted,
                allowed,
                override_original_block,
                effective_before,
                effective_after,
                effective_limit,
                slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                injured_player_id,
                flag,
                today);
        }
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, (uint32_t)team_id, today);
        }
        return adjusted;
    }

    uint8_t injury_slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t effective_count = 0u;
    uint32_t effective_limit = 0u;
    if (kbo_foreign_injury_replacement_signing_exception_available(
            (uint32_t)team_id,
            player,
            &injury_slot_type,
            &injured_player_id,
            &effective_count,
            &effective_limit)) {
        static volatile LONG injury_offer_log_count = 0;
        LONG slot = InterlockedIncrement(&injury_offer_log_count);
        uint8_t adjusted = original_result != 0u ? original_result : 1u;
        if (slot <= 120) {
            append_logf(
                "foreign injury replacement offer eligibility allowed player=%u requester_team=%d injured=%u slot=%s original=%u adjusted=%u effective=%u limit=%u flag=%d today=%u",
                player_id,
                team_id,
                injured_player_id,
                kbo_foreign_injury_slot_label(injury_slot_type),
                (uint32_t)original_result,
                (uint32_t)adjusted,
                effective_count,
                effective_limit,
                flag,
                today);
        }
        return adjusted;
    }

    return original_result;
}

__declspec(noinline) void ootp_kbo_fa_submit_offer_probe_wrapper(
    uintptr_t screen_ptr,
    uintptr_t original_func_ptr)
{
    if (screen_ptr != 0 && memory_range_readable((void*)screen_ptr, 0xc0)) {
        uint32_t today = 0;
        uint32_t holder_team_id = 0;
        uint32_t requester_team_id = 0;
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
            if (!recent_block || requester_team_id != holder_team_id) {
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
        }
    }

    OotpFaSubmitOfferProbeFn original_func = (OotpFaSubmitOfferProbeFn)original_func_ptr;
    if (original_func != NULL) {
        original_func((void*)screen_ptr);
    }
}

__declspec(noinline) int32_t ootp_kbo_ai_fa_status_candidate_insert_wrapper(
    uintptr_t frame_ptr,
    uintptr_t player_ptr,
    int32_t insert_index,
    uintptr_t candidate_array)
{
    if (frame_ptr == 0 || player_ptr == 0 || insert_index < 0) {
        return insert_index;
    }
    if (!memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        return insert_index;
    }

    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0;
    uint32_t holder_team_id = 0;
    uint32_t requester_team_id = 0;
    uintptr_t team_ptr = *(uintptr_t*)(frame_ptr + 0x140u);
    if (team_ptr != 0 && memory_range_readable((void*)(team_ptr + 0x4450u), sizeof(uint32_t))) {
        requester_team_id = *(uint32_t*)(team_ptr + 0x4450u);
    }

    if (player_id != 0u
        && requester_team_id != 0u
        && kbo_team_id_is_military_service_team(requester_team_id)) {
        static LONG military_ai_block_log_count = 0;
        LONG slot = InterlockedIncrement(&military_ai_block_log_count);
        if (slot <= 300) {
            append_logf(
                "military service team AI FA status candidate blocked player=%u requester_team=%u index=%d",
                player_id,
                requester_team_id,
                insert_index);
        }
        return insert_index;
    }

    if (player_id != 0u
        && requester_team_id != 0u
        && kbo_get_foreign_waiver_current_yyyymmdd(&today)
        && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u
        && holder_team_id != requester_team_id) {
        static LONG block_log_count = 0;
        LONG slot = InterlockedIncrement(&block_log_count);
        if (slot <= 300) {
            append_logf(
                "foreign reserve AI FA status candidate blocked player=%u requester_team=%u holder_team=%u index=%d today=%u",
                player_id,
                requester_team_id,
                holder_team_id,
                insert_index,
                today);
        }
        return insert_index;
    }

    if (player_id != 0u
            && requester_team_id != 0u
            && kbo_custom_foreign_policy_enabled()
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights((uint8_t*)player_ptr)) {
        if (today == 0u && !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
            today = 0u;
        }
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            requester_team_id,
            (uint8_t*)player_ptr,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            static LONG custom_policy_ai_block_log_count = 0;
            LONG slot = InterlockedIncrement(&custom_policy_ai_block_log_count);
            if (slot <= 300) {
                append_logf(
                    "custom foreign policy AI FA status candidate blocked player=%u requester_team=%u index=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    player_id,
                    requester_team_id,
                    insert_index,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    today);
            }
            kbo_record_recent_custom_foreign_policy_block(player_id, requester_team_id, today);
            return insert_index;
        }
    }

    if (candidate_array == 0) {
        return insert_index;
    }
    if (!memory_range_readable((void*)(candidate_array + ((uintptr_t)insert_index * sizeof(uintptr_t))), sizeof(uintptr_t))) {
        return insert_index;
    }
    *(uintptr_t*)(candidate_array + ((uintptr_t)insert_index * sizeof(uintptr_t))) = player_ptr;
    *(int32_t*)(frame_ptr - 0x2c) = insert_index + 1;
    return insert_index + 1;
}

