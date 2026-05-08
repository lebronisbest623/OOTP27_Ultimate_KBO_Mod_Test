/* Military service seed locks, draft queue, and return-history idempotency. Included from native/src/military_service_loan.inc. */

static void kbo_lock_military_service_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_military_service_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_military_service_seeds(void)
{
    InterlockedExchange(&g_kbo_military_service_seed_lock, 0);
}

static int kbo_find_military_draft_candidate_index(uint32_t player_id, uint16_t entry_year)
{
    if (player_id == 0u || entry_year == 0u) {
        return -1;
    }
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == player_id && candidate->entry_year == entry_year) {
            return (int)i;
        }
    }
    return -1;
}

static int kbo_queue_military_draft_candidate(
    uintptr_t player_ptr,
    uint32_t player_id,
    uint16_t entry_year,
    uint32_t original_team_id,
    uint32_t original_league_id,
    const char* source)
{
    if (player_ptr == 0 || player_id == 0u || entry_year == 0u || original_team_id == 0u) {
        return 0;
    }

    int existing = kbo_find_military_draft_candidate_index(player_id, entry_year);
    if (existing >= 0) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[existing];
        candidate->player_ptr = player_ptr;
        if (original_team_id != 0u) {
            candidate->original_team_id = original_team_id;
        }
        if (original_league_id != 0u) {
            candidate->original_league_id = original_league_id;
        }
        append_logf(
            "KBO military draft candidate refreshed source=%s player_id=%u year=%u original_team=%u original_league=%u selected=%u player=%p",
            source != NULL ? source : "",
            player_id,
            entry_year,
            candidate->original_team_id,
            candidate->original_league_id,
            (uint32_t)candidate->selected,
            (void*)player_ptr);
        return 1;
    }

    LONG slot = InterlockedIncrement(&g_kbo_military_draft_candidate_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_kbo_military_draft_candidate_count);
        return 0;
    }

    KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[slot];
    memset(candidate, 0, sizeof(*candidate));
    candidate->player_id = player_id;
    candidate->player_ptr = player_ptr;
    candidate->entry_year = entry_year;
    candidate->original_team_id = original_team_id;
    candidate->original_league_id = original_league_id;
    append_logf(
        "KBO military draft candidate queued source=%s slot=%ld player_id=%u year=%u original_team=%u original_league=%u player=%p",
        source != NULL ? source : "",
        slot,
        player_id,
        entry_year,
        original_team_id,
        original_league_id,
        (void*)player_ptr);
    return 1;
}

static int kbo_count_military_draft_candidates_for_year(uint16_t entry_year)
{
    if (entry_year == 0u) {
        return 0;
    }
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    int queued = 0;
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id != 0u && candidate->entry_year == entry_year && candidate->selected == 0u) {
            queued++;
        }
    }
    return queued;
}

static int kbo_mark_military_return_history_once(uint32_t player_id, uint32_t history_yyyymmdd)
{
    if (player_id == 0u || history_yyyymmdd == 0u) {
        return 0;
    }
    LONG count = g_kbo_military_return_history_key_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryReturnHistoryKey* key = &g_kbo_military_return_history_keys[i];
        if (key->player_id == player_id && key->history_yyyymmdd == history_yyyymmdd) {
            return 0;
        }
    }

    LONG slot = InterlockedIncrement(&g_kbo_military_return_history_key_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_kbo_military_return_history_key_count);
        return 0;
    }
    g_kbo_military_return_history_keys[slot].player_id = player_id;
    g_kbo_military_return_history_keys[slot].history_yyyymmdd = history_yyyymmdd;
    return 1;
}
