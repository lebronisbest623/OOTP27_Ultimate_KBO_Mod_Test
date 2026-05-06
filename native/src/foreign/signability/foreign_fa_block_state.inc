/* Recent foreign FA block cache. Included from native/src/foreign/foreign_signability_hooks.inc. */

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
