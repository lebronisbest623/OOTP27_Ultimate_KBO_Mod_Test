/* Automatic foreign reserve-right retain/skip decisions. Included from native/src/foreign_waiver_ai.inc. */

static int kbo_apply_ai_foreign_waiver_rules(
    uint32_t player_id,
    uint32_t player_current_team_id,
    int value_score,
    int forced,
    uint32_t target_team_id)
{
    if (target_team_id == 0 || player_id == 0) {
        return 0;
    }

    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(target_team_id, 1);
    if (destination_team == NULL) {
        append_logf("foreign waiver auto: target team not found target_team=%u", target_team_id);
        return 0;
    }

    uint32_t player_current_league_id = 0;
    uint8_t* player = kbo_find_player_by_id(player_id, &player_current_team_id, &player_current_league_id);
    if (player == NULL) {
        return 0;
    }

    uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t fallback_league = player_current_league_id != 0 ? player_current_league_id : destination_league;
    if (fallback_league == 0) {
        return 0;
    }

    if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, target_team_id)) {
        append_logf("foreign waiver auto: retain failed player=%u -> team=%u value=%d forced=%d",
            player_id, target_team_id, value_score, forced);
        return 0;
    }

    append_logf(
        "foreign waiver auto: retained player=%u -> team=%u value=%d forced=%d",
        player_id, target_team_id, value_score, forced);
    kbo_append_foreign_waiver_decision_record("ai", "RETAIN", target_team_id, player_id, value_score, forced, 1);
    return 1;
}

static int kbo_ai_foreign_waiver_should_retain(
    uint8_t* player,
    uint32_t player_id,
    uint32_t decision_team_id,
    int value_score,
    int forced,
    int32_t* out_threshold,
    const char** out_reason)
{
    if (out_threshold != NULL) {
        *out_threshold = 0;
    }
    if (out_reason != NULL) {
        *out_reason = "unknown";
    }
    if (player == NULL || player_id == 0u || decision_team_id == 0u) {
        if (out_reason != NULL) { *out_reason = "invalid_candidate"; }
        return 0;
    }
    if (forced) {
        if (out_reason != NULL) { *out_reason = "forced"; }
        return 1;
    }

    int32_t threshold = kbo_get_foreign_waiver_value_threshold_for_player(player);
    if (out_threshold != NULL) {
        *out_threshold = threshold;
    }
    if (value_score < threshold) {
        if (out_reason != NULL) { *out_reason = "below_value_threshold"; }
        return 0;
    }

    if (out_reason != NULL) { *out_reason = "value_threshold"; }
    return 1;
}

static void run_foreign_waiver_ai_core_once(void)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }
    {
        static LONG rights_loaded = 0;
        if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
    }

    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return;
    }
    append_logf(
        "foreign waiver auto: window check -> OPEN (auto window: %u~%u)",
        g_kbo_foreign_waiver_window_start_serial,
        g_kbo_foreign_waiver_window_end_serial);

    uint32_t target_team_id = kbo_get_foreign_waiver_auto_target_team_id();
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return;
    }
    uint32_t window_start = 0u;
    uint32_t window_end = 0u;
    kbo_current_foreign_waiver_window_dates(&window_start, &window_end);
    kbo_prune_expired_foreign_waiver_rights(today);

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_resolve_kbo_league_id();
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    int considered = 0;
    int retained = 0;
    int skipped = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        if (target_team_id != 0u && decision_team_id != target_team_id) {
            continue;
        }
        if (decision_team_id == 0u) {
            continue;
        }

        uint8_t* decision_team = find_kbo_team_by_numeric_id_any_league(decision_team_id, 1);
        if (decision_team == NULL) {
            continue;
        }
        uint32_t team_league_id = *(uint32_t*)(decision_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (configured_league_id != 0u && team_league_id != configured_league_id) {
            continue;
        }

        if (kbo_has_active_foreign_waiver_right(decision_team_id, player_id, today)) {
            continue;
        }
        if (kbo_foreign_waiver_decision_exists(window_end, decision_team_id, player_id)) {
            continue;
        }

        int forced = kbo_is_forced_foreign_candidate_id(player_id);
        int score = kbo_foreign_waiver_value_score(player);
        considered++;

        int32_t retain_threshold = 0;
        const char* retain_reason = "unknown";
        if (!kbo_ai_foreign_waiver_should_retain(
                player,
                player_id,
                decision_team_id,
                score,
                forced,
                &retain_threshold,
                &retain_reason)) {
            skipped++;
            append_logf(
                "foreign waiver auto: SKIP player=%u team=%u value=%d forced=%d threshold=%d reason=%s",
                player_id,
                decision_team_id,
                score,
                forced,
                retain_threshold,
                retain_reason);
            kbo_append_foreign_waiver_decision_record("ai", "SKIP", decision_team_id, player_id, score, forced, 0);
            continue;
        }

        if (kbo_apply_ai_foreign_waiver_rules(player_id, current_team_id, score, forced, decision_team_id)) {
            retained++;
        } else {
            skipped++;
            append_logf(
                "foreign waiver auto: SKIP player=%u team=%u value=%d forced=%d reason=retain_failed",
                player_id, decision_team_id, score, forced);
            kbo_append_foreign_waiver_decision_record("ai", "SKIP", decision_team_id, player_id, score, forced, 0);
        }
    }

    append_logf(
        "foreign waiver auto: all-player decisions considered=%d retained=%d skipped=%d target_team=%u",
        considered,
        retained,
        skipped,
        target_team_id);
}

