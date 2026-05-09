#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bootstrap/ootp_offsets.h"
#include "../bootstrap/profiler.h"
#include "../core/core_atomic_file.h"
#include "../core/core_current_date.h"
#include "../core/core_flags/flags_api.h"
#include "../core/core_league_context_parts/event_manager.h"
#include "../core/core_league_context_parts/league_context_lookup.h"
#include "../core/core_league_events.h"
#include "../core/core_log.h"
#include "../core/core_save_paths.h"
#include "../core/core_text_date.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/team_lookup.h"
#include "../team/team_roster_arrays.h"
#include "../team/team_string.h"
#include "foreign_csv_parse.h"
#include "foreign_priority_events.h"
#include "foreign_waiver_config.h"
#include "foreign_waiver_core.h"
#include "foreign_waiver_date.h"
#include "foreign_waiver_decisions.h"
#include "foreign_waiver_events.h"
#include "foreign_waiver_paths.h"
#include "foreign_waiver_player_eval.h"
#include "foreign_waiver_policy.h"
#include "injury/foreign_injury_labels.h"
#include "replacement_seed/foreign_replacement_seed.h"
#include "rights/foreign_waiver_rights_query.h"
#include "roster_audit/foreign_roster_audit.h"

#define KBO_FOREIGN_WAIVER_AI_AUTO_TEAM_SLOT_MAX 512

typedef struct KboForeignWaiverAiTargetCandidate {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t current_team_id;
    int score;
    int forced;
} KboForeignWaiverAiTargetCandidate;



/* ---- native/src/foreign/foreign_waiver_ai.inc ---- */
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
    uint32_t window_start = 0u;
    uint32_t window_end = 0u;
    kbo_current_foreign_waiver_window_dates(&window_start, &window_end);
    append_logf(
        "foreign waiver auto: window check -> OPEN (window: %u~%u)",
        window_start,
        window_end);

    uint32_t target_team_id = kbo_get_foreign_waiver_auto_target_team_id();
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return;
    }
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


/* ---- native/src/foreign/foreign_waiver_io.inc ---- */
/* Foreign waiver command and candidate CSV I/O. Included from native/src/foreign_waiver_ai.inc. */

static int get_kbo_foreign_waiver_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_waiver_candidates.csv", out, out_size);
}

static int append_foreign_waiver_candidate_csv_header(HANDLE file)
{
    const char* header = "date,source,player_id,current_team_id,active_team_id,original_team_id,current_league_id,priority_window_open,priority_team_id,priority_eligible,dfa_flag,restricted,secondary_restricted,loan_active,injury_active,foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}


static int is_csv_empty(HANDLE file)
{
    DWORD high = 0;
    uint32_t low = GetFileSize(file, &high);
    if (high != 0) {
        return 0;
    }
    return low == 0;
}


static void write_foreign_waiver_candidates(const char* source)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        static volatile LONG no_vector_log_count = 0;
        if (InterlockedIncrement(&no_vector_log_count) <= 5) {
            append_log_line("foreign waiver scanner: no player vector");
        }
        return;
    }

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_csv_path(path, sizeof(path))) {
        append_log_line("foreign waiver scanner: unable to resolve output path");
        return;
    }

    {
        char dir[MAX_PATH] = {0};
        size_t len = strlen(path);
        if (len > 0 && len < sizeof(dir)) {
            strcpy_s(dir, sizeof(dir), path);
            char* slash = strrchr(dir, '\\');
            if (slash != NULL) {
                *slash = '\0';
                CreateDirectoryA(dir, NULL);
            }
        }
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign waiver scanner: failed to open %s", path);
        return;
    }

    if (is_csv_empty(file)) {
        append_foreign_waiver_candidate_csv_header(file);
    }
    SetFilePointer(file, 0, NULL, FILE_END);

    char date[16] = {0};
    if (!kbo_current_history_date(date, sizeof(date), 2000, source)) {
        strcpy_s(date, sizeof(date), "00000000");
    }

    int priority_window_open = kbo_is_foreign_waiver_negotiation_window_open();

    int scanned = 0;
    int written_candidates = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }
        uint8_t dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        uint8_t restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        uint8_t loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        uint8_t inj_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];

        int forced_foreign = kbo_is_forced_foreign_candidate_id(player_id);
        int score = kbo_foreign_waiver_value_score(player);
        if (!forced_foreign && score <= 0) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t original_team_id = kbo_get_player_original_team_id(player);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        uint32_t priority_team_id = priority_window_open ? decision_team_id : 0;
        int priority_eligible = priority_window_open && decision_team_id != 0u;

        char line[320] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
            date,
            (source == NULL ? "" : source),
            player_id,
            current_team_id,
            active_team_id,
            original_team_id,
            current_league_id,
            (uint32_t)priority_window_open,
            priority_team_id,
            (uint32_t)priority_eligible,
            (uint32_t)dfa,
            (uint32_t)restricted,
            (uint32_t)secondary,
            (uint32_t)loan_active,
            (uint32_t)inj_active,
            score);

        DWORD written = 0;
        WriteFile(file, line, (DWORD)len, &written, NULL);
        written_candidates++;
    }

    CloseHandle(file);

    append_logf("foreign waiver scanner: scanned=%d candidates=%d file=%s", scanned, written_candidates, path);
}


/* ---- native/src/foreign/foreign_waiver_top_candidate.inc ---- */
/* Foreign reserve-right top-candidate resolver for UI. Included from native/src/foreign_waiver_ai.inc. */

int kbo_resolve_foreign_waiver_top_candidate_for_team(
    uint32_t team_id,
    uint32_t* out_player_id,
    uint32_t* out_current_team_id)
{
    if (team_id == 0u || out_player_id == NULL || out_current_team_id == NULL) {
        return 0;
    }
    *out_player_id = 0u;
    *out_current_team_id = 0u;
    {
        static LONG rights_loaded = 0;
        if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return 0;
    }
    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_resolve_kbo_league_id();
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int best_score = -1;
    uint32_t best_player_id = 0u;
    uint32_t best_current_team_id = 0u;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (player_id == 0u || decision_team_id != team_id
                || (current_league_id != 0u && current_league_id != configured_league_id)
                || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        int score = kbo_foreign_waiver_value_score(player);
        if (kbo_has_active_foreign_waiver_right(team_id, player_id, today)) {
            continue;
        }

        if (score > best_score) {
            best_score = score;
            best_player_id = player_id;
            best_current_team_id = current_team_id;
        }
    }

    if (best_player_id == 0u) {
        return 0;
    }
    *out_player_id = best_player_id;
    *out_current_team_id = best_current_team_id;
    return 1;
}


/* ---- native/src/foreign/foreign_waiver_scanner.inc ---- */
/* Foreign reserve-right background scanner thread. Included from native/src/foreign_waiver_ai.inc. */

static LONG g_kbo_foreign_waiver_scanner_started = 0;

static DWORD WINAPI kbo_foreign_waiver_scanner_thread(LPVOID parameter)
{
    (void)parameter;
    uint32_t tick = 0;
    uint32_t last_ai_run_date = 0u;
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(5000)) {
            break;
        }
        KBO_PROFILE_BEGIN(profile_foreign_waiver_scanner_tick);
        tick++;
        uint32_t today = 0u;
        char readiness_path[MAX_PATH] = {0};
        if (!kbo_get_current_yyyymmdd(&today)
                || !kbo_get_save_scoped_data_file("foreign_waiver_commands.txt", readiness_path, sizeof(readiness_path))) {
            static LONG waiting_logged = 0;
            if (InterlockedCompareExchange(&waiting_logged, 1, 0) == 0) {
                append_log_line("foreign waiver worker waiting: save path/date not ready");
            }
            KBO_PROFILE_END(profile_foreign_waiver_scanner_tick, "foreign_waiver.scanner.not_ready");
            continue;
        }

        process_foreign_waiver_commands();
        if (!kbo_is_foreign_waiver_negotiation_window_open()) {
            KBO_PROFILE_END(profile_foreign_waiver_scanner_tick, "foreign_waiver.scanner.window_closed");
            continue;
        }

        int background_scanner_enabled = read_kbo_localappdata_flag_file("enable_foreign_waiver_background_scanner.txt");
        if (background_scanner_enabled) {
            audit_foreign_roster_state("foreign_roster_pre_tick", 0);
        }
        if (today != last_ai_run_date) {
            run_foreign_waiver_ai_core_once();
            last_ai_run_date = today;
        }
        if (background_scanner_enabled && (tick % 6u) == 0u) {
            audit_foreign_roster_state("foreign_roster_post_tick", 1);
            write_foreign_waiver_candidates("foreign_waiver_scanner");
        }
        KBO_PROFILE_END(profile_foreign_waiver_scanner_tick, "foreign_waiver.scanner.tick");
    }
    InterlockedExchange(&g_kbo_foreign_waiver_scanner_started, 0);
    append_log_line("foreign waiver scanner thread stopped");
    return 0;
}

void start_kbo_foreign_waiver_scanner_thread(void)
{
    if (!kbo_foreign_waiver_ai_enabled()) {
        return;
    }
    int background_scanner_enabled = read_kbo_localappdata_flag_file("enable_foreign_waiver_background_scanner.txt");
    if (InterlockedCompareExchange(&g_kbo_foreign_waiver_scanner_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_waiver_scanner_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        if (background_scanner_enabled) {
            append_log_line("foreign waiver scanner thread started");
        } else {
            append_log_line("foreign waiver lightweight retain worker started; candidate scanner disabled");
        }
    } else {
        InterlockedExchange(&g_kbo_foreign_waiver_scanner_started, 0);
        append_log_line("foreign waiver scanner thread failed to start");
    }
}
