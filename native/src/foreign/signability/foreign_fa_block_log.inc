/* Foreign FA/signability diagnostic logging. */

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
