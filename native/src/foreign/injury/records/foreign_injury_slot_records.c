#include "../internal/foreign_injury_internal.h"

#include "../../../core/news/templates/core_news_templates.h"

int kbo_persist_foreign_injury_replacements_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("foreign injury replacement: persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }

    DWORD written = 0;
    const char* header = "team_id,league_id,injured_player_id,replacement_player_id,opened_on,expected_end,slot_type,status,converted\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        char line[256] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
            rec->team_id,
            rec->league_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            rec->opened_on_yyyymmdd,
            rec->expected_end_yyyymmdd,
            (uint32_t)rec->slot_type,
            (uint32_t)rec->status,
            (uint32_t)rec->converted);
        if (len > 0 && len < (int)sizeof(line)) {
            written = 0;
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
    }

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        append_logf("foreign injury replacement: atomic commit failed path=%s", path);
        return 0;
    }
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);
    return 1;
}

void kbo_ensure_foreign_injury_replacements_loaded(void)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_ensure);
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        KBO_PROFILE_END(profile_foreign_injury_ensure, "foreign_injury.ensure.no_path");
        return;
    }

    static DWORD last_empty_import_attempt_tick = 0u;
    kbo_lock_foreign_injury_replacements();
    int path_changed = strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) != 0;
    if (path_changed) {
        last_empty_import_attempt_tick = 0u;
        kbo_load_foreign_injury_replacements_locked(path);
    }
    DWORD now = GetTickCount();
    int should_import_seed = path_changed;
    if (!should_import_seed
            && g_kbo_foreign_injury_replacement_count == 0
            && (last_empty_import_attempt_tick == 0u
                || now < last_empty_import_attempt_tick
                || now - last_empty_import_attempt_tick > 60000u)) {
        should_import_seed = 1;
    }
    if (should_import_seed) {
        last_empty_import_attempt_tick = now;
        uint32_t today = 0u;
        kbo_get_current_yyyymmdd(&today);
        int imported = 0;
        char save_seed_path[MAX_PATH] = {0};
        char global_seed_path[MAX_PATH] = {0};
        if (kbo_get_save_foreign_injury_replacement_seed_path(save_seed_path, sizeof(save_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(save_seed_path, today, "save_seed");
        }
        if (kbo_get_global_foreign_injury_replacement_seed_path(global_seed_path, sizeof(global_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(global_seed_path, today, "global_seed");
        }
        if (imported > 0) {
            kbo_persist_foreign_injury_replacements_locked();
        }
    }
    kbo_unlock_foreign_injury_replacements();
    KBO_PROFILE_END(profile_foreign_injury_ensure, should_import_seed
        ? "foreign_injury.ensure.import_checked"
        : "foreign_injury.ensure.cached");
}

int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed)
{
    if (injured_player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->injured_player_id == injured_player_id
                && (include_closed || rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED)) {
            return i;
        }
    }
    return -1;
}

int kbo_team_has_foreign_injury_slot_locked(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    if (out_injured_player_id != NULL) {
        *out_injured_player_id = 0u;
    }
    if (team_id == 0u || slot_type == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->team_id == team_id
                && rec->slot_type == slot_type
                && kbo_foreign_injury_status_uses_slot(rec->status)) {
            if (out_injured_player_id != NULL) {
                *out_injured_player_id = rec->injured_player_id;
            }
            return 1;
        }
    }
    return 0;
}

int kbo_team_has_foreign_injury_slot(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    result = kbo_team_has_foreign_injury_slot_locked(team_id, slot_type, out_injured_player_id);
    kbo_unlock_foreign_injury_replacements();
    return result;
}

int kbo_team_has_foreign_injury_slot_for_candidate_locked(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_replacement_player_id != NULL) { *out_replacement_player_id = 0u; }
    if (team_id == 0u || slot_type == 0u || candidate_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->team_id != team_id
                || rec->slot_type != slot_type
                || !kbo_foreign_injury_status_uses_slot(rec->status)
                || rec->injured_player_id == candidate_player_id) {
            continue;
        }
        if (rec->replacement_player_id != 0u && rec->replacement_player_id != candidate_player_id) {
            continue;
        }
        if (out_injured_player_id != NULL) {
            *out_injured_player_id = rec->injured_player_id;
        }
        if (out_replacement_player_id != NULL) {
            *out_replacement_player_id = rec->replacement_player_id;
        }
        return 1;
    }
    return 0;
}

int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_slot_candidate);
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    result = kbo_team_has_foreign_injury_slot_for_candidate_locked(
        team_id,
        slot_type,
        candidate_player_id,
        out_injured_player_id,
        out_replacement_player_id);
    kbo_unlock_foreign_injury_replacements();
    KBO_PROFILE_END(profile_foreign_injury_slot_candidate, result
        ? "foreign_injury.slot_candidate.hit"
        : "foreign_injury.slot_candidate.miss");
    return result;
}

void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed)
{
    if (out_open != NULL) { *out_open = 0; }
    if (out_pending != NULL) { *out_pending = 0; }
    if (out_closed != NULL) { *out_closed = 0; }
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (team_id != 0u && rec->team_id != team_id) {
            continue;
        }
        if (kbo_foreign_injury_status_uses_slot(rec->status)) {
            if (out_open != NULL) { (*out_open)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_PENDING) {
            if (out_pending != NULL) { (*out_pending)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            if (out_closed != NULL) { (*out_closed)++; }
        }
    }
    kbo_unlock_foreign_injury_replacements();
}

void kbo_emit_foreign_injury_replacement_news(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase)
{
    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u || rec->league_id == 0u) {
        return;
    }

    uint32_t event_date = 0u;
    if (!kbo_get_current_yyyymmdd(&event_date) || event_date == 0u) {
        event_date = rec->opened_on_yyyymmdd;
    }
    if (event_date == 0u) {
        return;
    }

    char player_name[96] = {0};
    uint8_t* player = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
    if (player != NULL) {
        kbo_copy_player_display_name(player, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", rec->injured_player_id);
    }

    char title[128] = {0};
    char body[1024] = {0};
    char team_link[96] = {0};
    char injured_player_link[144] = {0};
    char replacement_player_link[144] = {0};
    char days_left_text[16] = {0};
    snprintf(team_link, sizeof(team_link), "<Team #%u:team#%u>", rec->team_id, rec->team_id);
    snprintf(injured_player_link, sizeof(injured_player_link), "<%s:player#%u>", player_name, rec->injured_player_id);
    snprintf(days_left_text, sizeof(days_left_text), "%d", days_left > 0 ? days_left : 0);

    const char* title_key = "foreign_injury.open.title";
    const char* body_key = "foreign_injury.open.body";
    if (phase != NULL && strcmp(phase, "closed") == 0) {
        char replacement_name[96] = {0};
        uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
        if (replacement != NULL) {
            kbo_copy_player_display_name(replacement, replacement_name, sizeof(replacement_name));
        }
        if (replacement_name[0] == '\0' && rec->replacement_player_id != 0u) {
            snprintf(replacement_name, sizeof(replacement_name), "Player #%u", rec->replacement_player_id);
        }
        if (rec->replacement_player_id != 0u) {
            snprintf(replacement_player_link, sizeof(replacement_player_link), "<%s:player#%u>", replacement_name, rec->replacement_player_id);
            title_key = "foreign_injury.closed_with_replacement.title";
            body_key = "foreign_injury.closed_with_replacement.body";
        } else {
            title_key = "foreign_injury.closed_without_replacement.title";
            body_key = "foreign_injury.closed_without_replacement.body";
        }
    } else if (phase != NULL && strcmp(phase, "pending") == 0) {
        title_key = "foreign_injury.pending.title";
        body_key = "foreign_injury.pending.body";
    }

    KboNewsTemplateVar vars[] = {
        { "team_link", team_link },
        { "injured_player_link", injured_player_link },
        { "replacement_player_link", replacement_player_link },
        { "days_left", days_left_text },
        { "slot_label", kbo_foreign_injury_slot_label(rec->slot_type) },
    };
    if (!kbo_news_template_render_key(
            title_key,
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            phase != NULL ? phase : "foreign_injury")
            || !kbo_news_template_render_key(
                body_key,
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                phase != NULL ? phase : "foreign_injury")) {
        append_logf(
            "foreign injury replacement: news skipped phase=%s team=%u injured=%u league=%u reason=template_unavailable",
            phase != NULL ? phase : "open",
            rec->team_id,
            rec->injured_player_id,
            rec->league_id);
        return;
    }

    int created = create_kbo_native_live_news_with_body(
        event_date / 10000u,
        (event_date / 100u) % 100u,
        event_date % 100u,
        rec->league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "foreign injury replacement: news phase=%s team=%u injured=%u league=%u created=%d",
        phase != NULL ? phase : "open",
        rec->team_id,
        rec->injured_player_id,
        rec->league_id,
        created);
}

