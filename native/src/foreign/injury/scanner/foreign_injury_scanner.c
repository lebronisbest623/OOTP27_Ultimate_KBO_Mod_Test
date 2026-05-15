#include "foreign_injury_scanner_internal.h"
#include "../../common/policy/foreign_player_policy.h"

static LONG g_kbo_foreign_injury_return_wait_log_count = 0;
static LONG g_kbo_foreign_injury_non_roster_log_count = 0;
static LONG g_kbo_foreign_injury_below_min_log_count = 0;
static int kbo_foreign_injury_replacement_scan_source_is_read_only(const char* source)
{
    return source != NULL && (strcmp(source, "foreign_policy_webview") == 0
        || strcmp(source, "foreign_policy_text") == 0 || strcmp(source, "hotkey_text") == 0
        || strcmp(source, "foreign_slot_cache") == 0);
}

void kbo_foreign_injury_replacement_scan_once(const char* source)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        kbo_rule_audit_emit_fields("foreign_injury.replacement.scan", "skip", "disabled", source, NULL);
        return;
    }

    if (kbo_foreign_injury_replacement_scan_source_is_read_only(source)) {
        kbo_ensure_foreign_injury_replacements_loaded();
        return;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        kbo_rule_audit_emit_fields("foreign_injury.replacement.scan", "skip", "date_unavailable", source, NULL);
        return;
    }

    kbo_ensure_foreign_injury_replacements_loaded();

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.scan",
                "skip",
                "player_vector_unavailable",
                source,
                &audit_fields);
        } while (0);
        return;
    }

    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_resolve_kbo_league_id();
    }

    int scanned = 0;
    int opened = 0;
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
        scanned++;

        uint8_t injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        int16_t days_left = *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
        int direct_injury_eligible = kbo_foreign_injury_duration_meets_minimum(days_left, min_days);

        uint32_t team_id = 0u;
        uint32_t league_id = 0u;
        int has_assignment = kbo_foreign_injury_resolve_player_team_assignment(
            player,
            player_id,
            configured_league_id,
            &team_id,
            &league_id);
        int inactive_roster_present = !direct_injury_eligible && has_assignment
            ? kbo_foreign_injury_player_on_inactive_replacement_roster(player, player_id, team_id, today)
            : 0;
        int inactive_roster_eligible = kbo_foreign_injury_inactive_roster_has_long_term_injury_basis(
            injury_active,
            days_left,
            min_days,
            inactive_roster_present);
        if (!direct_injury_eligible && !inactive_roster_eligible) {
            if (injury_active != 0u || days_left > 0 || inactive_roster_present) {
                LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_below_min_log_count);
                if (log_slot <= 80 || (log_slot % 250) == 0) {
                    uint32_t current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                    uint32_t active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
                    uint32_t loan_team = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
                    uint32_t original_team = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
                    uint32_t default_team = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
                        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
                        : 0u;
                    do {
                        KboLogFields audit_fields;
                        kbo_log_fields_init(&audit_fields);
                        kbo_log_field_u32(&audit_fields, "date", today);
                        kbo_log_field_u32(&audit_fields, "player_id", player_id);
                        kbo_log_field_u32(&audit_fields, "team_id", team_id);
                        kbo_log_field_u32(&audit_fields, "league_id", league_id);
                        kbo_log_field_u32(&audit_fields, "current_team_id", current_team);
                        kbo_log_field_u32(&audit_fields, "active_team_id", active_team);
                        kbo_log_field_u32(&audit_fields, "loan_team_id", loan_team);
                        kbo_log_field_u32(&audit_fields, "original_team_id", original_team);
                        kbo_log_field_u32(&audit_fields, "default_team_id", default_team);
                        kbo_log_field_u32(&audit_fields, "injury_active", (uint32_t)injury_active);
                        kbo_log_field_i32(&audit_fields, "days_left", (int)days_left);
                        kbo_log_field_i32(&audit_fields, "min_days", min_days);
                        kbo_log_field_u32(&audit_fields, "inactive_roster", inactive_roster_present ? 1u : 0u);
                        kbo_log_field_u32(&audit_fields, "assignment", has_assignment ? 1u : 0u);
                        kbo_rule_audit_emit_fields(
                            "foreign_injury.replacement.lifecycle",
                            "skip_candidate",
                            inactive_roster_present
                                ? "inactive_roster_without_long_term_injury_days"
                                : "injury_below_min_days",
                            source,
                            &audit_fields);
                    } while (0);
                    kbo_log_runtimef(
                        "foreign injury replacement: skipped below-min injury candidate source=%s player=%u current=%u active=%u loan=%u original=%u default=%u resolved_team=%u league=%u assignment=%d injury=%u days_left=%d min_days=%d inactive_roster=%d",
                        source != NULL ? source : "",
                        player_id,
                        current_team,
                        active_team,
                        loan_team,
                        original_team,
                        default_team,
                        team_id,
                        league_id,
                        has_assignment,
                        (uint32_t)injury_active,
                        (int)days_left,
                        min_days,
                        inactive_roster_present);
                }
            }
            continue;
        }
        int effective_days_left = days_left;
        if (effective_days_left < min_days) {
            effective_days_left = min_days;
        }
        if (!has_assignment || !kbo_foreign_injury_player_has_baseball_position(player)) {
            LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_non_roster_log_count);
            if (log_slot <= 60 || (log_slot % 100) == 0) {
                kbo_log_runtimef(
                    "foreign injury replacement: skipped non-roster injury candidate source=%s player=%u current=%u active=%u loan=%u original=%u default=%u resolved_team=%u league=%u position=%u role=%u assignment=%d injury=%u days_left=%d inactive_roster=%d",
                    source != NULL ? source : "",
                    player_id,
                    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
                    memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
                        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
                        : 0u,
                    team_id,
                    league_id,
                    (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
                    (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
                    has_assignment,
                    (uint32_t)injury_active,
                    (int)days_left,
                    inactive_roster_eligible);
            }
            continue;
        }

        KboForeignInjuryReplacement created_rec;
        memset(&created_rec, 0, sizeof(created_rec));
        int created = 0;
        kbo_lock_foreign_injury_replacements();
        int existing = kbo_find_foreign_injury_replacement_locked(player_id, 0);
        if (existing < 0 && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++];
            rec->team_id = team_id;
            rec->league_id = league_id != 0u ? league_id : configured_league_id;
            rec->injured_player_id = player_id;
            rec->replacement_player_id = 0u;
            rec->opened_on_yyyymmdd = today;
            rec->expected_end_yyyymmdd = direct_injury_eligible
                ? kbo_add_days_yyyymmdd(today, (uint32_t)days_left)
                : 0u;
            rec->slot_type = kbo_foreign_injury_slot_type_for_player(player);
            rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
            rec->converted = 0u;
            created_rec = *rec;
            created = kbo_persist_foreign_injury_replacements_locked();
        }
        kbo_unlock_foreign_injury_replacements();

        if (created) {
            opened++;
            kbo_emit_foreign_injury_replacement_news(
                &created_rec,
                effective_days_left,
                direct_injury_eligible ? "open" : "open_roster");
                        do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "team_id", created_rec.team_id);
                kbo_log_field_u32(&audit_fields, "league_id", created_rec.league_id);
                kbo_log_field_u32(&audit_fields, "injured_player_id", created_rec.injured_player_id);
                kbo_log_field_u32(&audit_fields, "injury_active", (uint32_t)injury_active);
                kbo_log_field_i32(&audit_fields, "days_left", (int)days_left);
                kbo_log_field_i32(&audit_fields, "min_days", min_days);
                kbo_log_field_i32(&audit_fields, "effective_days_left", effective_days_left);
                kbo_log_field_u32(&audit_fields, "inactive_roster", inactive_roster_eligible ? 1u : 0u);
                kbo_log_field_u32(&audit_fields, "slot_type", (uint32_t)created_rec.slot_type);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "open_slot",
                    direct_injury_eligible ? "injury_min_days_met" : "inactive_roster_long_term_il",
                    source,
                    &audit_fields);
            } while (0);
            kbo_log_runtimef(
                "foreign injury replacement: opened source=%s team=%u player=%u league=%u days_left=%d effective_days_left=%d trigger=%s slot=%s",
                source != NULL ? source : "",
                created_rec.team_id,
                created_rec.injured_player_id,
                created_rec.league_id,
                (int)days_left,
                effective_days_left,
                direct_injury_eligible ? "injury_fields" : "inactive_roster",
                kbo_foreign_injury_slot_label(created_rec.slot_type));
        }
    }

    KboForeignInjuryClosedNews closed_news[16];
    int closed_count = 0;
    KboForeignInjuryReplacement active_news[16];
    int active_count = 0;
    int changed = 0;
    memset(closed_news, 0, sizeof(closed_news));
    memset(active_news, 0, sizeof(active_news));
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            continue;
        }
        int uses_slot = kbo_foreign_injury_status_uses_slot(rec->status);
        if (!uses_slot && rec->status != KBO_FOREIGN_INJURY_STATUS_PENDING) {
            continue;
        }

        uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        if (uses_slot) {
            kbo_foreign_injury_restore_active_replacement_player(rec, source);
        }
        if (uses_slot && rec->replacement_player_id == 0u) {
            uint32_t replacement_player_id = kbo_foreign_injury_resolve_replacement_for_record(rec);
            if (replacement_player_id != 0u) {
                rec->replacement_player_id = replacement_player_id;
                rec->status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
                uses_slot = 1;
                if (active_count < (int)(sizeof(active_news) / sizeof(active_news[0]))) {
                    active_news[active_count++] = *rec;
                }
                changed = 1;
            }
        }

        if (!kbo_foreign_injury_injured_player_returned_to_top_team(rec, injured)) {
            if (injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] == 0u) {
                LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_return_wait_log_count);
                if (log_slot <= 80 || (log_slot % 100) == 0) {
                    uint8_t* wait_team = find_kbo_team_by_numeric_id_any_league(rec->team_id, 1);
                    int active_roster = kbo_foreign_injury_team_active_roster_contains_player(wait_team, rec->injured_player_id);
                    int inactive_roster = kbo_foreign_injury_team_inactive_roster_contains_player(wait_team, rec->injured_player_id);
                    kbo_log_runtimef(
                        "foreign injury replacement: waiting top-team return source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d",
                        source != NULL ? source : "",
                        rec->team_id,
                        rec->injured_player_id,
                        rec->replacement_player_id,
                        *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                        *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                        *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                        rec->league_id,
                        (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                        (uint32_t)injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET],
                        (int)*(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET),
                        active_roster,
                        inactive_roster);
                }
            }
            continue;
        }

        uint32_t replacement_player_id = kbo_foreign_injury_resolve_replacement_for_record(rec);
        if (replacement_player_id != 0u) {
            rec->replacement_player_id = replacement_player_id;
        }
        KboForeignInjuryReplacementDecision decision;
        memset(&decision, 0, sizeof(decision));
        const char* close_phase = "closed_without_replacement";
        if (rec->replacement_player_id != 0u) {
            uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
            kbo_foreign_injury_choose_returning_player(rec, injured, replacement, &decision);
            if (decision.choice == KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT) {
                kbo_foreign_injury_release_injured_player(
                    rec->team_id,
                    rec->injured_player_id,
                    source);
                rec->converted = 1u;
                close_phase = "closed_keep_replacement";
            } else {
                kbo_foreign_injury_release_replacement_player(
                    rec->team_id,
                    rec->replacement_player_id,
                    source);
                rec->converted = 0u;
                close_phase = "closed_keep_injured";
            }
        } else {
            kbo_foreign_injury_choose_returning_player(rec, injured, NULL, &decision);
            rec->converted = 0u;
        }
        rec->status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
        if (closed_count < (int)(sizeof(closed_news) / sizeof(closed_news[0]))) {
            closed_news[closed_count].rec = *rec;
            closed_news[closed_count].decision = decision;
            snprintf(closed_news[closed_count].phase, sizeof(closed_news[closed_count].phase), "%s", close_phase);
            closed_count++;
        }
        changed = 1;
    }
    if (changed) {
        kbo_persist_foreign_injury_replacements_locked();
    }
    kbo_unlock_foreign_injury_replacements();

    for (int i = 0; i < active_count; i++) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_u32(&audit_fields, "team_id", active_news[i].team_id);
            kbo_log_field_u32(&audit_fields, "league_id", active_news[i].league_id);
            kbo_log_field_u32(&audit_fields, "injured_player_id", active_news[i].injured_player_id);
            kbo_log_field_u32(&audit_fields, "replacement_player_id", active_news[i].replacement_player_id);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.lifecycle",
                "activate_slot",
                "replacement_resolved",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: activated source=%s team=%u injured=%u replacement=%u league=%u",
            source != NULL ? source : "",
            active_news[i].team_id,
            active_news[i].injured_player_id,
            active_news[i].replacement_player_id,
            active_news[i].league_id);
    }

    kbo_foreign_injury_emit_closed_news_batch(closed_news, closed_count, today, source);

    if (opened > 0 || active_count > 0 || closed_count > 0) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_i32(&audit_fields, "scanned_foreign", scanned);
            kbo_log_field_i32(&audit_fields, "opened", opened);
            kbo_log_field_i32(&audit_fields, "activated", active_count);
            kbo_log_field_i32(&audit_fields, "closed", closed_count);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.scan",
                "process",
                "lifecycle_changes",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: scan source=%s scanned_foreign=%d opened=%d active=%d pending=%d closed=%d",
            source != NULL ? source : "",
            scanned,
            opened,
            active_count,
            0,
            closed_count);
    }
}
