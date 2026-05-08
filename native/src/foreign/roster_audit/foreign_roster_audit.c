#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_current_date.h"
#include "../../core/core_flags/flags_api.h"
#include "../../core/core_log.h"
#include "../../core/core_save_paths.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/team_lookup.h"
#include "../foreign_waiver_player_eval.h"
#include "foreign_roster_audit_paths.h"

static int kbo_foreign_roster_audit_csv_empty(HANDLE file)
{
    DWORD high = 0;
    uint32_t low = GetFileSize(file, &high);
    if (high != 0) {
        return 0;
    }
    return low == 0;
}

static uint32_t kbo_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
}

/* Foreign roster audit and snapshots. Included from native/src/foreign_waiver_ai.inc. */

#define KBO_FOREIGN_ROSTER_AUDIT_MAX 8192

typedef struct KboForeignRosterAuditState {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t current_league_id;
    uint32_t loan_team_id;
    uint8_t restricted;
    uint8_t secondary_restricted;
    uint8_t dfa;
    uint8_t loan_active;
    uint8_t injury_active;
    int score;
    uint32_t seen_generation;
} KboForeignRosterAuditState;

static KboForeignRosterAuditState g_kbo_foreign_roster_audit[KBO_FOREIGN_ROSTER_AUDIT_MAX] = {{0}};
static int g_kbo_foreign_roster_audit_count = 0;
static uint32_t g_kbo_foreign_roster_audit_generation = 0u;
static char g_kbo_foreign_roster_audit_save_path[MAX_PATH] = {0};


/* Foreign roster audit state capture and comparison helpers. Included from foreign_roster_audit.inc. */

static void kbo_capture_foreign_roster_audit_state(uint8_t* player, KboForeignRosterAuditState* out)
{
    memset(out, 0, sizeof(*out));
    out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->original_team_id = kbo_get_player_original_team_id(player);
    out->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    out->loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    out->restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    out->secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    out->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    out->loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
    out->injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    out->score = kbo_foreign_waiver_value_score(player);
}

static int kbo_foreign_roster_audit_state_changed(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    return old_state->nation_id != new_state->nation_id
        || old_state->current_team_id != new_state->current_team_id
        || old_state->active_team_id != new_state->active_team_id
        || old_state->original_team_id != new_state->original_team_id
        || old_state->current_league_id != new_state->current_league_id
        || old_state->loan_team_id != new_state->loan_team_id
        || old_state->restricted != new_state->restricted
        || old_state->secondary_restricted != new_state->secondary_restricted
        || old_state->dfa != new_state->dfa
        || old_state->loan_active != new_state->loan_active
        || old_state->injury_active != new_state->injury_active
        || old_state->score != new_state->score;
}

static const char* kbo_foreign_roster_audit_change_type(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (old_state == NULL) {
        return "NEW_FOREIGN";
    }
    if (old_state->current_team_id != 0u && new_state->current_team_id == 0u
            && old_state->active_team_id != 0u && new_state->active_team_id == 0u) {
        return "RELEASE_OBSERVED";
    }
    if (old_state->current_team_id != 0u && new_state->current_team_id == 0u) {
        return "CURRENT_TEAM_CLEARED";
    }
    if (old_state->active_team_id != 0u && new_state->active_team_id == 0u) {
        return "ACTIVE_TEAM_CLEARED";
    }
    if (old_state->current_team_id == 0u && new_state->current_team_id != 0u) {
        return "CURRENT_TEAM_ASSIGNED";
    }
    if (old_state->restricted != new_state->restricted
            || old_state->secondary_restricted != new_state->secondary_restricted
            || old_state->dfa != new_state->dfa
            || old_state->loan_active != new_state->loan_active
            || old_state->injury_active != new_state->injury_active) {
        return "STATUS_CHANGED";
    }
    if (old_state->current_league_id != new_state->current_league_id
            || old_state->loan_team_id != new_state->loan_team_id) {
        return "ASSIGNMENT_CHANGED";
    }
    return "STATE_CHANGED";
}

static KboForeignRosterAuditState* kbo_find_foreign_roster_audit_state(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < g_kbo_foreign_roster_audit_count; i++) {
        if (g_kbo_foreign_roster_audit[i].player_id == player_id) {
            return &g_kbo_foreign_roster_audit[i];
        }
    }
    return NULL;
}

/* Foreign roster audit CSV writers. Included from foreign_roster_audit.inc. */

static int append_foreign_roster_audit_csv_header(HANDLE file)
{
    const char* header =
        "date,source,change_type,player_id,nation_id,"
        "old_current_team_id,new_current_team_id,"
        "old_active_team_id,new_active_team_id,"
        "old_original_team_id,new_original_team_id,"
        "old_current_league_id,new_current_league_id,"
        "old_loan_team_id,new_loan_team_id,"
        "old_restricted,new_restricted,"
        "old_secondary_restricted,new_secondary_restricted,"
        "old_dfa,new_dfa,"
        "old_loan_active,new_loan_active,"
        "old_injury_active,new_injury_active,"
        "old_foreign_value_score,new_foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

static int append_foreign_roster_snapshot_csv_header(HANDLE file)
{
    const char* header =
        "date,source,player_id,nation_id,current_team_id,active_team_id,original_team_id,"
        "current_league_id,loan_team_id,restricted,secondary_restricted,dfa,loan_active,"
        "injury_active,foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

static HANDLE kbo_open_foreign_roster_audit_append_file(void)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_roster_audit_csv_path(path, sizeof(path))) {
        append_log_line("foreign roster audit: unable to resolve audit output path");
        return INVALID_HANDLE_VALUE;
    }

    DWORD attrs = GetFileAttributesA(path);
    int needs_header = (attrs == INVALID_FILE_ATTRIBUTES);
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ | FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign roster audit: failed to open audit file path=%s gle=%lu", path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }
    if (needs_header || kbo_foreign_roster_audit_csv_empty(file)) {
        append_foreign_roster_audit_csv_header(file);
    }
    return file;
}

static void kbo_write_foreign_roster_audit_change(
    HANDLE file,
    const char* date,
    const char* source,
    const char* change_type,
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (file == INVALID_HANDLE_VALUE || new_state == NULL) {
        return;
    }

    KboForeignRosterAuditState zero = {0};
    if (old_state == NULL) {
        old_state = &zero;
    }

    char line[512] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d\r\n",
        date != NULL ? date : "",
        source != NULL ? source : "",
        change_type != NULL ? change_type : "",
        new_state->player_id,
        new_state->nation_id,
        old_state->current_team_id,
        new_state->current_team_id,
        old_state->active_team_id,
        new_state->active_team_id,
        old_state->original_team_id,
        new_state->original_team_id,
        old_state->current_league_id,
        new_state->current_league_id,
        old_state->loan_team_id,
        new_state->loan_team_id,
        (uint32_t)old_state->restricted,
        (uint32_t)new_state->restricted,
        (uint32_t)old_state->secondary_restricted,
        (uint32_t)new_state->secondary_restricted,
        (uint32_t)old_state->dfa,
        (uint32_t)new_state->dfa,
        (uint32_t)old_state->loan_active,
        (uint32_t)new_state->loan_active,
        (uint32_t)old_state->injury_active,
        (uint32_t)new_state->injury_active,
        old_state->score,
        new_state->score);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

static void kbo_write_foreign_roster_snapshot_row(
    HANDLE file,
    const char* date,
    const char* source,
    const KboForeignRosterAuditState* state)
{
    if (file == INVALID_HANDLE_VALUE || state == NULL) {
        return;
    }

    char line[320] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d\r\n",
        date != NULL ? date : "",
        source != NULL ? source : "",
        state->player_id,
        state->nation_id,
        state->current_team_id,
        state->active_team_id,
        state->original_team_id,
        state->current_league_id,
        state->loan_team_id,
        (uint32_t)state->restricted,
        (uint32_t)state->secondary_restricted,
        (uint32_t)state->dfa,
        (uint32_t)state->loan_active,
        (uint32_t)state->injury_active,
        state->score);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

static HANDLE kbo_open_foreign_roster_snapshot_file(void)
{
    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_roster_snapshot_csv_path(path, sizeof(path))) {
        append_log_line("foreign roster audit: unable to resolve snapshot output path");
        return INVALID_HANDLE_VALUE;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign roster audit: failed to open snapshot file path=%s gle=%lu", path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }
    append_foreign_roster_snapshot_csv_header(file);
    return file;
}


/* Foreign roster audit scanner. Included from foreign_roster_audit.inc. */

void audit_foreign_roster_state(const char* source, int write_snapshot)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        static volatile LONG no_vector_log_count = 0;
        if (InterlockedIncrement(&no_vector_log_count) <= 5) {
            append_log_line("foreign roster audit: no player vector");
        }
        return;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        strcpy_s(save_path, sizeof(save_path), "unknown_save");
    }
    if (strcmp(g_kbo_foreign_roster_audit_save_path, save_path) != 0) {
        g_kbo_foreign_roster_audit_count = 0;
        g_kbo_foreign_roster_audit_generation = 0u;
        memset(g_kbo_foreign_roster_audit, 0, sizeof(g_kbo_foreign_roster_audit));
        snprintf(g_kbo_foreign_roster_audit_save_path, sizeof(g_kbo_foreign_roster_audit_save_path), "%s", save_path);
        append_logf("foreign roster audit: baseline reset save=%s source=%s", save_path, source != NULL ? source : "");
    }

    g_kbo_foreign_roster_audit_generation++;
    if (g_kbo_foreign_roster_audit_generation == 0u) {
        g_kbo_foreign_roster_audit_generation = 1u;
        for (int i = 0; i < g_kbo_foreign_roster_audit_count; i++) {
            g_kbo_foreign_roster_audit[i].seen_generation = 0u;
        }
    }

    int baseline_scan = g_kbo_foreign_roster_audit_count == 0 && g_kbo_foreign_roster_audit_generation == 1u;
    int write_snapshot_now = write_snapshot || baseline_scan;

    char date[16] = {0};
    if (!kbo_current_history_date(date, sizeof(date), 2000, source)) {
        strcpy_s(date, sizeof(date), "00000000");
    }

    HANDLE audit_file = INVALID_HANDLE_VALUE;
    HANDLE snapshot_file = write_snapshot_now ? kbo_open_foreign_roster_snapshot_file() : INVALID_HANDLE_VALUE;

    int scanned = 0;
    int foreign = 0;
    int rostered = 0;
    int active_retained = 0;
    int free_or_unassigned = 0;
    int changed = 0;
    int new_foreign = 0;
    int current_cleared = 0;
    int active_cleared = 0;
    static volatile LONG release_detail_log_count = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;
        if (!kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        KboForeignRosterAuditState current = {0};
        kbo_capture_foreign_roster_audit_state(player, &current);
        if (current.player_id == 0u) {
            continue;
        }

        foreign++;
        if (current.current_team_id != 0u) {
            rostered++;
        } else if (current.active_team_id != 0u) {
            active_retained++;
        } else {
            free_or_unassigned++;
        }

        if (snapshot_file != INVALID_HANDLE_VALUE) {
            kbo_write_foreign_roster_snapshot_row(snapshot_file, date, source, &current);
        }

        KboForeignRosterAuditState* previous = kbo_find_foreign_roster_audit_state(current.player_id);
        if (previous == NULL) {
            if (g_kbo_foreign_roster_audit_count < KBO_FOREIGN_ROSTER_AUDIT_MAX) {
                previous = &g_kbo_foreign_roster_audit[g_kbo_foreign_roster_audit_count++];
                *previous = current;
                previous->seen_generation = g_kbo_foreign_roster_audit_generation;
                if (!baseline_scan) {
                    if (audit_file == INVALID_HANDLE_VALUE) {
                        audit_file = kbo_open_foreign_roster_audit_append_file();
                    }
                    kbo_write_foreign_roster_audit_change(
                        audit_file,
                        date,
                        source,
                        "NEW_FOREIGN",
                        NULL,
                        &current);
                    changed++;
                    new_foreign++;
                }
            } else {
                static volatile LONG full_log_count = 0;
                if (InterlockedIncrement(&full_log_count) <= 5) {
                    append_logf(
                        "foreign roster audit: state table full max=%d player=%u",
                        KBO_FOREIGN_ROSTER_AUDIT_MAX,
                        current.player_id);
                }
            }
            continue;
        }

        KboForeignRosterAuditState old = *previous;
        previous->seen_generation = g_kbo_foreign_roster_audit_generation;
        if (!kbo_foreign_roster_audit_state_changed(&old, &current)) {
            continue;
        }

        const char* change_type = kbo_foreign_roster_audit_change_type(&old, &current);
        if (audit_file == INVALID_HANDLE_VALUE) {
            audit_file = kbo_open_foreign_roster_audit_append_file();
        }
        kbo_write_foreign_roster_audit_change(audit_file, date, source, change_type, &old, &current);

        if (old.current_team_id != 0u && current.current_team_id == 0u) {
            current_cleared++;
            LONG slot = InterlockedIncrement(&release_detail_log_count);
            if (slot <= 240) {
                append_logf(
                    "foreign roster audit: %s player=%u nation=%u current=%u->%u active=%u->%u original=%u->%u restricted=%u->%u secondary=%u->%u dfa=%u->%u loan=%u->%u injury=%u->%u score=%d->%d date=%s source=%s",
                    change_type,
                    current.player_id,
                    current.nation_id,
                    old.current_team_id,
                    current.current_team_id,
                    old.active_team_id,
                    current.active_team_id,
                    old.original_team_id,
                    current.original_team_id,
                    (uint32_t)old.restricted,
                    (uint32_t)current.restricted,
                    (uint32_t)old.secondary_restricted,
                    (uint32_t)current.secondary_restricted,
                    (uint32_t)old.dfa,
                    (uint32_t)current.dfa,
                    (uint32_t)old.loan_active,
                    (uint32_t)current.loan_active,
                    (uint32_t)old.injury_active,
                    (uint32_t)current.injury_active,
                    old.score,
                    current.score,
                    date,
                    source != NULL ? source : "");
            }
        }
        if (old.active_team_id != 0u && current.active_team_id == 0u) {
            active_cleared++;
        }

        current.seen_generation = g_kbo_foreign_roster_audit_generation;
        *previous = current;
        changed++;
    }

    if (audit_file != INVALID_HANDLE_VALUE) {
        CloseHandle(audit_file);
    }
    if (snapshot_file != INVALID_HANDLE_VALUE) {
        CloseHandle(snapshot_file);
    }

    if (baseline_scan || changed > 0) {
        append_logf(
            "foreign roster audit: source=%s date=%s scanned=%d foreign=%d rostered=%d active_retained=%d free=%d changed=%d new=%d current_cleared=%d active_cleared=%d snapshot=%d save=%s",
            source != NULL ? source : "",
            date,
            scanned,
            foreign,
            rostered,
            active_retained,
            free_or_unassigned,
            changed,
            new_foreign,
            current_cleared,
            active_cleared,
            write_snapshot_now,
            g_kbo_foreign_roster_audit_save_path);
    }
}

