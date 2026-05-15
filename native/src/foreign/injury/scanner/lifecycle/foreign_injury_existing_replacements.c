#include "../foreign_injury_scanner_internal.h"

static LONG g_kbo_foreign_injury_return_wait_log_count = 0;

void kbo_foreign_injury_process_existing_replacements(
    uint32_t today,
    const char* source,
    int* out_active_count,
    int* out_closed_count)
{
    if (out_active_count != NULL) {
        *out_active_count = 0;
    }
    if (out_closed_count != NULL) {
        *out_closed_count = 0;
    }

    KboForeignInjuryClosedNews closed_news[16];
    int closed_count = 0;
    int invalid_closed_count = 0;
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
        if (uses_slot && !kbo_foreign_injury_record_has_minimum_injury_basis(rec)) {
            uint32_t replacement_player_id = kbo_foreign_injury_resolve_replacement_for_record(rec);
            if (replacement_player_id != 0u) {
                rec->replacement_player_id = replacement_player_id;
                kbo_foreign_injury_release_replacement_player(
                    rec->team_id,
                    replacement_player_id,
                    source);
            }
            rec->converted = 0u;
            rec->status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
            invalid_closed_count++;
            changed = 1;
            do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "team_id", rec->team_id);
                kbo_log_field_u32(&audit_fields, "league_id", rec->league_id);
                kbo_log_field_u32(&audit_fields, "injured_player_id", rec->injured_player_id);
                kbo_log_field_u32(&audit_fields, "replacement_player_id", rec->replacement_player_id);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "close_slot",
                    "invalid_no_long_term_injury_basis",
                    source,
                    &audit_fields);
            } while (0);
            kbo_log_runtimef(
                "foreign injury replacement: closed invalid slot source=%s team=%u injured=%u replacement=%u league=%u reason=no_long_term_injury_basis",
                source != NULL ? source : "",
                rec->team_id,
                rec->injured_player_id,
                rec->replacement_player_id,
                rec->league_id);
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

        int returned_to_top_team = kbo_foreign_injury_injured_player_returned_to_top_team(rec, injured);
        int expected_end_due = !returned_to_top_team
            && kbo_foreign_injury_expected_end_reached(today, rec->expected_end_yyyymmdd);
        if (!returned_to_top_team && !expected_end_due) {
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
        if (expected_end_due) {
            kbo_log_runtimef(
                "foreign injury replacement: expected-end close fallback source=%s team=%u injured=%u replacement=%u today=%u expected_end=%u",
                source != NULL ? source : "",
                rec->team_id,
                rec->injured_player_id,
                rec->replacement_player_id,
                today,
                rec->expected_end_yyyymmdd);
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
    if (out_active_count != NULL) {
        *out_active_count = active_count;
    }
    if (out_closed_count != NULL) {
        *out_closed_count = closed_count + invalid_closed_count;
    }
}
