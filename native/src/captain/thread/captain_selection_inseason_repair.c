#include "../internal/captain_selection_internal.h"
#include "../../core/logging/rule_audit.h"

int kbo_run_captain_inseason_repair_once(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const char* source)
{
    KboCaptainSelectionRow current_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(current_rows, 0, sizeof(current_rows));
    int current_count = kbo_captain_load_selection_csv(season, current_rows, KBO_CAPTAIN_MAX_TEAMS);
    if (current_count <= 0) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", date);
            kbo_log_field_u32(&audit_fields, "season", season);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "current_rows", current_count);
            kbo_rule_audit_emit_fields(
                "captain.inseason_repair",
                "write_initial_selection",
                "current_rows_unavailable",
                source,
                &audit_fields);
        } while (0);
        return kbo_captain_write_initial_selection(
            date,
            season,
            league_id,
            source != NULL ? source : "captain_inseason_bootstrap");
    }

    uint32_t team_ids[KBO_CAPTAIN_MAX_TEAMS] = {0};
    int team_count = 0;
    int missing_count = 0;
    int departed_count = 0;
    if (!kbo_captain_current_rows_need_inseason_repair(
            league_id,
            current_rows,
            current_count,
            team_ids,
            &team_count,
            &missing_count,
            &departed_count)) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", date);
            kbo_log_field_u32(&audit_fields, "season", season);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "current_rows", current_count);
            kbo_log_field_i32(&audit_fields, "teams", team_count);
            kbo_log_field_i32(&audit_fields, "missing", missing_count);
            kbo_log_field_i32(&audit_fields, "departed", departed_count);
            kbo_rule_audit_emit_fields(
                "captain.inseason_repair",
                "skip",
                "current_rows_valid",
                source,
                &audit_fields);
        } while (0);
        return 0;
    }

    KboCaptainSelectionRow candidate_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(candidate_rows, 0, sizeof(candidate_rows));
    int selected_count = 0;
    int candidate_count = kbo_captain_select_for_preseason(
        date,
        season,
        league_id,
        candidate_rows,
        KBO_CAPTAIN_MAX_TEAMS,
        &selected_count);
    if (candidate_count <= 0) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", date);
            kbo_log_field_u32(&audit_fields, "season", season);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "teams", team_count);
            kbo_log_field_i32(&audit_fields, "missing", missing_count);
            kbo_log_field_i32(&audit_fields, "departed", departed_count);
            kbo_log_field_i32(&audit_fields, "candidate_rows", candidate_count);
            kbo_log_field_i32(&audit_fields, "selected", selected_count);
            kbo_rule_audit_emit_fields(
                "captain.inseason_repair",
                "skip",
                "no_candidates",
                source,
                &audit_fields);
        } while (0);
        append_logf(
            "KBO captain in-season repair skipped source=%s date=%u season=%u league_id=%u reason=no_candidates missing=%d departed=%d",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            missing_count,
            departed_count);
        return 0;
    }

    KboCaptainSelectionRow merged_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(merged_rows, 0, sizeof(merged_rows));
    KboCaptainSelectionRow news_old_rows[KBO_CAPTAIN_MAX_TEAMS];
    KboCaptainSelectionRow news_new_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(news_old_rows, 0, sizeof(news_old_rows));
    memset(news_new_rows, 0, sizeof(news_new_rows));
    int merged_count = 0;
    int repaired_count = 0;
    int still_missing_count = 0;
    int news_count = 0;
    for (int i = 0; i < team_count && merged_count < KBO_CAPTAIN_MAX_TEAMS; i++) {
        uint32_t team_id = team_ids[i];
        int current_index = kbo_captain_find_row_index_by_team(current_rows, current_count, team_id);
        int candidate_index = kbo_captain_find_row_index_by_team(candidate_rows, candidate_count, team_id);
        int current_valid = current_index >= 0
            && kbo_captain_existing_row_still_with_team(&current_rows[current_index]);

        if (current_valid) {
            merged_rows[merged_count++] = current_rows[current_index];
            continue;
        }

        if (candidate_index >= 0) {
            merged_rows[merged_count] = candidate_rows[candidate_index];
            if (merged_rows[merged_count].player_id != 0u) {
                if (current_index >= 0 && current_rows[current_index].player_id != 0u) {
                    snprintf(
                        merged_rows[merged_count].reason,
                        sizeof(merged_rows[merged_count].reason),
                        "inseason_replacement:departed:%u",
                        current_rows[current_index].player_id);
                } else {
                    snprintf(
                        merged_rows[merged_count].reason,
                        sizeof(merged_rows[merged_count].reason),
                        "inseason_replacement:missing_row");
                }
                repaired_count++;
                                do {
                    KboLogFields audit_fields;
                    kbo_log_fields_init(&audit_fields);
                    kbo_log_field_u32(&audit_fields, "date", date);
                    kbo_log_field_u32(&audit_fields, "season", season);
                    kbo_log_field_u32(&audit_fields, "league_id", league_id);
                    kbo_log_field_u32(&audit_fields, "team_id", team_id);
                    kbo_log_field_u32(&audit_fields, "old_player_id", current_index >= 0 ? current_rows[current_index].player_id : 0u);
                    kbo_log_field_u32(&audit_fields, "new_player_id", merged_rows[merged_count].player_id);
                    kbo_log_field_i32(&audit_fields, "new_score", merged_rows[merged_count].score);
                    kbo_log_field_i32(&audit_fields, "missing", missing_count);
                    kbo_log_field_i32(&audit_fields, "departed", departed_count);
                    kbo_rule_audit_emit_fields(
                        "captain.inseason_repair.team",
                        "replace_captain",
                        current_index >= 0 && current_rows[current_index].player_id != 0u
                        ? "departed_current_captain"
                        : "missing_current_row",
                        source,
                        &audit_fields);
                } while (0);
                if (news_count < KBO_CAPTAIN_MAX_TEAMS) {
                    if (current_index >= 0) {
                        news_old_rows[news_count] = current_rows[current_index];
                    }
                    news_new_rows[news_count] = merged_rows[merged_count];
                    news_count++;
                }
            } else {
                still_missing_count++;
            }
            merged_count++;
        } else if (current_index >= 0) {
            merged_rows[merged_count++] = current_rows[current_index];
            still_missing_count++;
        }
    }

    if (repaired_count <= 0) {
        if (still_missing_count > 0) {
            static uint32_t last_unresolved_date = 0u;
            static uint32_t last_unresolved_season = 0u;
            static uint32_t last_unresolved_league_id = 0u;
            static int last_unresolved_missing = -1;
            static int last_unresolved_departed = -1;
            static int last_unresolved_count = -1;
            if (date != last_unresolved_date
                    || season != last_unresolved_season
                    || league_id != last_unresolved_league_id
                    || missing_count != last_unresolved_missing
                    || departed_count != last_unresolved_departed
                    || still_missing_count != last_unresolved_count) {
                append_logf(
                    "KBO captain in-season repair skipped source=%s date=%u season=%u league_id=%u reason=unresolved_no_changes missing=%d departed=%d unresolved=%d",
                    source != NULL ? source : "",
                    date,
                    season,
                    league_id,
                    missing_count,
                    departed_count,
                    still_missing_count);
                                do {
                    KboLogFields audit_fields;
                    kbo_log_fields_init(&audit_fields);
                    kbo_log_field_u32(&audit_fields, "date", date);
                    kbo_log_field_u32(&audit_fields, "season", season);
                    kbo_log_field_u32(&audit_fields, "league_id", league_id);
                    kbo_log_field_i32(&audit_fields, "teams", team_count);
                    kbo_log_field_i32(&audit_fields, "missing", missing_count);
                    kbo_log_field_i32(&audit_fields, "departed", departed_count);
                    kbo_log_field_i32(&audit_fields, "unresolved", still_missing_count);
                    kbo_rule_audit_emit_fields(
                        "captain.inseason_repair",
                        "skip",
                        "unresolved_no_changes",
                        source,
                        &audit_fields);
                } while (0);
                last_unresolved_date = date;
                last_unresolved_season = season;
                last_unresolved_league_id = league_id;
                last_unresolved_missing = missing_count;
                last_unresolved_departed = departed_count;
                last_unresolved_count = still_missing_count;
            }
        }
        return 0;
    }

    char csv_path[MAX_PATH] = {0};
    int wrote = kbo_captain_write_selection_csv(
        merged_rows,
        merged_count,
        source != NULL ? source : "captain_inseason_repair",
        csv_path,
        sizeof(csv_path));
    if (wrote) {
        append_logf(
            "KBO captain in-season repair written source=%s date=%u season=%u league_id=%u repaired=%d missing=%d departed=%d unresolved=%d csv=%s",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            repaired_count,
            missing_count,
            departed_count,
            still_missing_count,
            csv_path);
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", date);
            kbo_log_field_u32(&audit_fields, "season", season);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "teams", team_count);
            kbo_log_field_i32(&audit_fields, "merged", merged_count);
            kbo_log_field_i32(&audit_fields, "repaired", repaired_count);
            kbo_log_field_i32(&audit_fields, "missing", missing_count);
            kbo_log_field_i32(&audit_fields, "departed", departed_count);
            kbo_log_field_i32(&audit_fields, "unresolved", still_missing_count);
            kbo_log_field_i32(&audit_fields, "news", news_count);
            kbo_rule_audit_emit_fields(
                "captain.inseason_repair",
                "write_selection_csv",
                "repaired_captains",
                source,
                &audit_fields);
        } while (0);
        for (int i = 0; i < news_count; i++) {
            kbo_emit_captain_replacement_news(
                date,
                season,
                league_id,
                news_old_rows[i].player_id != 0u ? &news_old_rows[i] : NULL,
                &news_new_rows[i],
                source != NULL ? source : "captain_inseason_repair");
        }
    } else {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", date);
            kbo_log_field_u32(&audit_fields, "season", season);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "teams", team_count);
            kbo_log_field_i32(&audit_fields, "merged", merged_count);
            kbo_log_field_i32(&audit_fields, "repaired", repaired_count);
            kbo_log_field_i32(&audit_fields, "missing", missing_count);
            kbo_log_field_i32(&audit_fields, "departed", departed_count);
            kbo_log_field_i32(&audit_fields, "unresolved", still_missing_count);
            kbo_rule_audit_emit_fields(
                "captain.inseason_repair",
                "fail",
                "write_csv_failed",
                source,
                &audit_fields);
        } while (0);
    }
    return wrote;
}
