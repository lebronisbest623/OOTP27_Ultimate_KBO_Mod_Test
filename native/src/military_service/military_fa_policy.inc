/* Military-service FA and player-action policy helpers. Included from native/src/military_service_loan.inc. */

static volatile LONG g_kbo_military_fa_block_player_id = 0;
static volatile LONG g_kbo_military_fa_block_requester_team_id = 0;
static volatile LONG g_kbo_military_fa_block_date = 0;
static volatile LONG64 g_kbo_military_fa_block_tick = 0;

static uint32_t kbo_military_policy_current_yyyymmdd(void)
{
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        kbo_get_current_yyyymmdd(&today);
    }
    return today;
}

static void kbo_record_recent_military_fa_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today)
{
    if (player_id == 0u || requester_team_id == 0u || today == 0u) {
        return;
    }

    InterlockedExchange(&g_kbo_military_fa_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_military_fa_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_military_fa_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_military_fa_block_tick, (LONG64)GetTickCount64());
}

static int kbo_recent_military_fa_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id)
{
    if (player_id == 0u || today == 0u) {
        return 0;
    }

    LONG cached_player = InterlockedCompareExchange(&g_kbo_military_fa_block_player_id, 0, 0);
    LONG cached_date = InterlockedCompareExchange(&g_kbo_military_fa_block_date, 0, 0);
    LONG64 cached_tick = InterlockedCompareExchange64(&g_kbo_military_fa_block_tick, 0, 0);
    if ((uint32_t)cached_player != player_id || (uint32_t)cached_date != today) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (cached_tick <= 0 || now < (ULONGLONG)cached_tick || now - (ULONGLONG)cached_tick > 120000ull) {
        return 0;
    }

    LONG requester = InterlockedCompareExchange(&g_kbo_military_fa_block_requester_team_id, 0, 0);
    if (requester <= 0 || !kbo_team_id_is_military_service_team((uint32_t)requester)) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = (uint32_t)requester;
    }
    return 1;
}

static int kbo_military_fa_candidate_fast_block(
    uintptr_t player_ptr,
    uint32_t requester_team_id,
    const char* context,
    uint32_t* out_player_id)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (!kbo_fix_enabled() || !kbo_team_id_is_military_service_team(requester_team_id)) {
        return 0;
    }
    if (context != NULL && strcmp(context, "signability") == 0) {
        return 0;
    }

    uint32_t player_id = 0u;
    if (player_ptr != 0
            && memory_range_readable((void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    }
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }

    uint32_t today = kbo_military_policy_current_yyyymmdd();
    kbo_record_recent_military_fa_block(player_id, requester_team_id, today);

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

static int kbo_military_offer_eligibility_should_block(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t flag,
    uint8_t original_result,
    uint32_t* out_player_id)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (team_id <= 0 || !kbo_team_id_is_military_service_team((uint32_t)team_id)) {
        return 0;
    }
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }

    uint32_t today = kbo_military_policy_current_yyyymmdd();
    kbo_record_recent_military_fa_block(player_id, (uint32_t)team_id, today);

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
    return 1;
}

static int kbo_military_signability_should_block(
    uint32_t player_id,
    int32_t requesting_team_id,
    int original_signability,
    uintptr_t caller_rva)
{
    if (player_id == 0u
            || requesting_team_id <= 0
            || !kbo_team_id_is_military_service_team((uint32_t)requesting_team_id)) {
        return 0;
    }

    uint32_t today = kbo_military_policy_current_yyyymmdd();
    kbo_record_recent_military_fa_block(player_id, (uint32_t)requesting_team_id, today);

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
    return 1;
}

static int kbo_military_submit_offer_should_block(uintptr_t screen_ptr, uint32_t player_id, uint32_t today)
{
    uint32_t military_team_id = 0u;
    if (!kbo_recent_military_fa_block_matches(player_id, today, &military_team_id)) {
        return 0;
    }

    static LONG military_submit_block_log_count = 0;
    LONG block_slot = InterlockedIncrement(&military_submit_block_log_count);
    if (block_slot <= 200) {
        append_logf(
            "military service team submit-offer blocked: screen=%p player=%u requester_team=%u today=%u",
            (void*)screen_ptr,
            player_id,
            military_team_id,
            today);
    }
    return 1;
}

static int kbo_military_ai_fa_candidate_should_block(
    uint32_t player_id,
    uint32_t requester_team_id,
    int32_t insert_index)
{
    if (player_id == 0u
            || requester_team_id == 0u
            || !kbo_team_id_is_military_service_team(requester_team_id)) {
        return 0;
    }

    static LONG military_ai_block_log_count = 0;
    LONG slot = InterlockedIncrement(&military_ai_block_log_count);
    if (slot <= 300) {
        append_logf(
            "military service team AI FA status candidate blocked player=%u requester_team=%u index=%d",
            player_id,
            requester_team_id,
            insert_index);
    }
    return 1;
}

#define KBO_PLAYER_ACTION_CONTEXT_SCAN_BYTES 0x300u

static uint8_t* kbo_military_player_action_context_find_player(
    uintptr_t action_context,
    uint32_t* out_offset,
    uint32_t* out_player_id)
{
    if (out_offset != NULL) { *out_offset = 0xffffffffu; }
    if (out_player_id != NULL) { *out_player_id = 0u; }
    if (action_context == 0
            || !memory_range_readable((void*)action_context, KBO_PLAYER_ACTION_CONTEXT_SCAN_BYTES)) {
        return NULL;
    }

    for (uint32_t offset = 0; offset + sizeof(uintptr_t) <= KBO_PLAYER_ACTION_CONTEXT_SCAN_BYTES; offset += sizeof(uintptr_t)) {
        uintptr_t candidate = *(uintptr_t*)(action_context + offset);
        if (!kbo_player_pointer_plausible(candidate)) {
            continue;
        }

        uint8_t* player = (uint8_t*)candidate;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || player_id > 200000000u) {
            continue;
        }
        if (out_offset != NULL) { *out_offset = offset; }
        if (out_player_id != NULL) { *out_player_id = player_id; }
        return player;
    }
    return NULL;
}

static uint32_t kbo_military_player_action_context_find_team_id(
    uintptr_t action_context,
    uint32_t* out_offset,
    uintptr_t* out_team_ptr)
{
    if (out_offset != NULL) { *out_offset = 0xffffffffu; }
    if (out_team_ptr != NULL) { *out_team_ptr = 0; }
    if (action_context == 0
            || !memory_range_readable((void*)action_context, KBO_PLAYER_ACTION_CONTEXT_SCAN_BYTES)) {
        return 0u;
    }

    for (uint32_t offset = 0; offset + sizeof(uintptr_t) <= KBO_PLAYER_ACTION_CONTEXT_SCAN_BYTES; offset += sizeof(uintptr_t)) {
        uintptr_t candidate = *(uintptr_t*)(action_context + offset);
        if (candidate == 0
                || !memory_range_readable(
                    (void*)(candidate + OOTP27_KBO_TEAM_ID_OFFSET),
                    sizeof(uint32_t))) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(candidate + OOTP27_KBO_TEAM_ID_OFFSET);
        if (!kbo_team_id_is_military_service_team(team_id)) {
            continue;
        }
        if (out_offset != NULL) { *out_offset = offset; }
        if (out_team_ptr != NULL) { *out_team_ptr = candidate; }
        return team_id;
    }

    return 0u;
}

static int kbo_military_player_action_should_block(
    uintptr_t action_context,
    int32_t action_id,
    uint8_t strict_check)
{
    uint32_t player_offset = 0xffffffffu;
    uint32_t player_id = 0u;
    uint8_t* player = kbo_military_player_action_context_find_player(action_context, &player_offset, &player_id);
    uint32_t team_offset = 0xffffffffu;
    uintptr_t team_ptr = 0;
    uint32_t military_team_id = kbo_military_player_action_context_find_team_id(action_context, &team_offset, &team_ptr);
    if (player == NULL || military_team_id == 0u) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
    if (current_team_id != 0u
            || military_active != 0u
            || action_id < 0x20
            || action_id > 0x80) {
        return 0;
    }

    static LONG military_action_block_log_count = 0;
    LONG slot = InterlockedIncrement(&military_action_block_log_count);
    if (slot <= 160) {
        append_logf(
            "military service team player action blocked: context=%p action=0x%x strict=%u player=%u team=%u player_off=0x%x team_off=0x%x team_ptr=%p",
            (void*)action_context,
            action_id,
            (unsigned)strict_check,
            player_id,
            military_team_id,
            player_offset,
            team_offset,
            (void*)team_ptr);
    }
    return 1;
}
