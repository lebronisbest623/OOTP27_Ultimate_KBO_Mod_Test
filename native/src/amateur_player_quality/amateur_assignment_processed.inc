static uint32_t kbo_amateur_assignment_processed_hash_key(uint32_t player_id, uint32_t team_id)
{
    uint32_t hash = 2166136261u;
    hash = (hash ^ player_id) * 16777619u;
    hash = (hash ^ team_id) * 16777619u;
    hash ^= hash >> 16;
    return hash;
}

static void kbo_amateur_assignment_clear_processed_cache(void)
{
    memset(g_kbo_amateur_assignment_processed, 0, sizeof(g_kbo_amateur_assignment_processed));
    memset(g_kbo_amateur_assignment_processed_hash, 0, sizeof(g_kbo_amateur_assignment_processed_hash));
    InterlockedExchange(&g_kbo_amateur_assignment_processed_count, 0);
    InterlockedExchange(&g_kbo_amateur_assignment_processed_hash_count, 0);
}

static int kbo_amateur_assignment_already_processed(uint32_t player_id, uint32_t team_id)
{
    if (player_id == 0u || team_id == 0u) {
        return 0;
    }

    uint32_t start = kbo_amateur_assignment_processed_hash_key(player_id, team_id)
        & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK;
    for (uint32_t probe = 0; probe < KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX; probe++) {
        KboAmateurAssignmentProcessed* slot =
            &g_kbo_amateur_assignment_processed_hash[(start + probe) & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK];
        if (slot->player_id == 0u) {
            return 0;
        }
        if (slot->player_id == player_id && slot->team_id == team_id) {
            return 1;
        }
    }
    return 1;
}

static void kbo_amateur_assignment_mark_processed(uint32_t player_id, uint32_t team_id)
{
    if (player_id == 0u || team_id == 0u) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_amateur_assignment_processed_hash_count, 0, 0)
            > (LONG)(KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX * 3 / 4)) {
        kbo_amateur_assignment_clear_processed_cache();
    }

    uint32_t start = kbo_amateur_assignment_processed_hash_key(player_id, team_id)
        & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK;
    for (uint32_t probe = 0; probe < KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX; probe++) {
        KboAmateurAssignmentProcessed* hash_slot =
            &g_kbo_amateur_assignment_processed_hash[(start + probe) & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK];
        if (hash_slot->player_id == player_id && hash_slot->team_id == team_id) {
            return;
        }
        if (hash_slot->player_id == 0u) {
            hash_slot->player_id = player_id;
            hash_slot->team_id = team_id;
            InterlockedIncrement(&g_kbo_amateur_assignment_processed_hash_count);
            break;
        }
    }

    LONG slot = InterlockedIncrement(&g_kbo_amateur_assignment_processed_count) - 1;
    if (slot < 0) {
        return;
    }
    if (slot >= KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX) {
        kbo_amateur_assignment_clear_processed_cache();
        slot = InterlockedIncrement(&g_kbo_amateur_assignment_processed_count) - 1;
    }
    g_kbo_amateur_assignment_processed[slot].player_id = player_id;
    g_kbo_amateur_assignment_processed[slot].team_id = team_id;
}
