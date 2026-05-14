#include "foreign_injury_scanner_internal.h"

static LONG g_kbo_foreign_injury_return_wait_log_count = 0;

int kbo_foreign_injury_player_matches_team(uint8_t* player, uint32_t team_id)
{
    if (player == NULL || team_id == 0u || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == team_id
        || *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == team_id;
}

int kbo_foreign_injury_candidate_matches_slot(uint8_t* player, uint8_t slot_type)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    uint8_t player_slot = kbo_foreign_injury_slot_type_for_player(player);
    return player_slot == slot_type;
}

int kbo_foreign_injury_team_active_roster_contains_player(uint8_t* team, uint32_t player_id)
{
    if (team == NULL
            || player_id == 0u
            || !memory_range_readable(team + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    uint32_t* active_ids = (uint32_t*)(team + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (active_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_foreign_injury_injured_player_returned_to_top_team(
    const KboForeignInjuryReplacement* rec,
    uint8_t* injured)
{
    if (rec == NULL || injured == NULL || rec->team_id == 0u
            || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    if (injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u) {
        return 0;
    }
    int16_t days_left = *(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
    if (days_left > 0) {
        return 0;
    }
    if (injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u) {
        return 0;
    }

    uint32_t injured_player_id = *(uint32_t*)(injured + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (current_team_id != rec->team_id) {
        return 0;
    }
    if (active_team_id != 0u && active_team_id != rec->team_id) {
        return 0;
    }
    if (rec->league_id != 0u && current_league_id != 0u && current_league_id != rec->league_id) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (rec->league_id != 0u && team_league_id != 0u && team_league_id != rec->league_id) {
        return 0;
    }
    if (!kbo_foreign_injury_team_active_roster_contains_player(team, injured_player_id)) {
        return 0;
    }

    return 1;
}

uint32_t kbo_foreign_injury_resolve_replacement_for_record(const KboForeignInjuryReplacement* rec)
{
    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u) {
        return 0u;
    }
    if (rec->replacement_player_id != 0u) {
        return rec->replacement_player_id;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0u;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u
                || player_id == rec->injured_player_id
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || !kbo_foreign_replacement_player_seed_matches_loaded(player, NULL)
                || !kbo_foreign_injury_player_matches_team(player, rec->team_id)
                || !kbo_foreign_injury_candidate_matches_slot(player, rec->slot_type)) {
            continue;
        }

        uint8_t injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        if (injury_active) {
            continue;
        }
        return player_id;
    }

    return 0u;
}

void kbo_foreign_injury_replacement_scan_once(const char* source)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        return;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        return;
    }

    kbo_ensure_foreign_injury_replacements_loaded();

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
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
        if (!injury_active || days_left < KBO_FOREIGN_INJURY_REPLACEMENT_MIN_DAYS) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (team_id == 0u) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
        if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            if (league_id == 0u) {
                league_id = team_league_id;
            }
        }
        if (configured_league_id != 0u && league_id != 0u && league_id != configured_league_id) {
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
            rec->expected_end_yyyymmdd = kbo_add_days_yyyymmdd(today, (uint32_t)days_left);
            rec->slot_type = kbo_foreign_injury_slot_type_for_player(player);
            rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
            rec->converted = 0u;
            created_rec = *rec;
            created = kbo_persist_foreign_injury_replacements_locked();
        }
        kbo_unlock_foreign_injury_replacements();

        if (created) {
            opened++;
            kbo_emit_foreign_injury_replacement_news(&created_rec, (int)days_left, "open");
            append_logf(
                "foreign injury replacement: opened source=%s team=%u player=%u league=%u days_left=%d slot=%s",
                source != NULL ? source : "",
                created_rec.team_id,
                created_rec.injured_player_id,
                created_rec.league_id,
                (int)days_left,
                kbo_foreign_injury_slot_label(created_rec.slot_type));
        }
    }

    KboForeignInjuryReplacement pending_news[16];
    int pending_count = 0;
    KboForeignInjuryReplacement active_news[16];
    int active_count = 0;
    int changed = 0;
    memset(pending_news, 0, sizeof(pending_news));
    memset(active_news, 0, sizeof(active_news));
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            continue;
        }
        if (!kbo_foreign_injury_status_uses_slot(rec->status)) {
            continue;
        }

        uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        kbo_foreign_injury_restore_active_replacement_player(rec, source);
        if (rec->replacement_player_id == 0u) {
            uint32_t replacement_player_id = kbo_foreign_injury_resolve_replacement_for_record(rec);
            if (replacement_player_id != 0u) {
                rec->replacement_player_id = replacement_player_id;
                rec->status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
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
                    append_logf(
                        "foreign injury replacement: waiting top-team return source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d",
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
                        active_roster);
                }
            }
            continue;
        }

        uint32_t replacement_player_id = kbo_foreign_injury_resolve_replacement_for_record(rec);
        if (replacement_player_id != 0u) {
            rec->replacement_player_id = replacement_player_id;
        }
        rec->status = KBO_FOREIGN_INJURY_STATUS_PENDING;
        rec->converted = 0u;
        if (pending_count < (int)(sizeof(pending_news) / sizeof(pending_news[0]))) {
            pending_news[pending_count++] = *rec;
        }
        changed = 1;
    }
    if (changed) {
        kbo_persist_foreign_injury_replacements_locked();
    }
    kbo_unlock_foreign_injury_replacements();

    for (int i = 0; i < active_count; i++) {
        append_logf(
            "foreign injury replacement: activated source=%s team=%u injured=%u replacement=%u league=%u",
            source != NULL ? source : "",
            active_news[i].team_id,
            active_news[i].injured_player_id,
            active_news[i].replacement_player_id,
            active_news[i].league_id);
    }

    for (int i = 0; i < pending_count; i++) {
        kbo_emit_foreign_injury_replacement_news(&pending_news[i], 0, "pending");
        append_logf(
            "foreign injury replacement: pending source=%s team=%u injured=%u replacement=%u league=%u",
            source != NULL ? source : "",
            pending_news[i].team_id,
            pending_news[i].injured_player_id,
            pending_news[i].replacement_player_id,
            pending_news[i].league_id);
    }

    if (opened > 0 || active_count > 0 || pending_count > 0) {
        append_logf(
            "foreign injury replacement: scan source=%s scanned_foreign=%d opened=%d active=%d pending=%d closed=%d",
            source != NULL ? source : "",
            scanned,
            opened,
            active_count,
            pending_count,
            0);
    }
}

DWORD WINAPI kbo_foreign_injury_replacement_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_foreign_injury_replacement_scan_once("foreign_injury_replacement_thread_start");
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(7000)) {
            break;
        }
        kbo_foreign_injury_replacement_scan_once("foreign_injury_replacement_thread");
    }
    InterlockedExchange(&g_kbo_foreign_injury_replacement_thread_started, 0);
    append_log_line("foreign injury replacement thread stopped");
    return 0;
}

void start_kbo_foreign_injury_replacement_thread(void)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        append_log_line("foreign injury replacement: disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_foreign_injury_replacement_thread_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_foreign_injury_replacement_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "foreign injury replacement scanner");
        append_log_line("foreign injury replacement thread started");
    } else {
        InterlockedExchange(&g_kbo_foreign_injury_replacement_thread_started, 0);
        append_log_line("foreign injury replacement thread failed to start");
    }
}

