#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/news/ledger/core_news_ledger.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../calendar/military_service_date.h"
#include "../players/state/military_player_state.h"
#include "../seed/parse/military_service_seed_parse.h"
#include "../selection/events/military_selection_event.h"
#include "../selection/events/policy/military_selection_policy.h"
#include "../selection/news/military_selection_news.h"
#include "military_return_preview_news.h"
#include "preview/military_return_preview_news_helpers.h"

static volatile LONG g_kbo_military_return_preview_last_checked_date = 0;

int kbo_emit_military_return_preview_news_if_due(uint32_t today_serial, const char* source)
{
    if (today_serial == 0u) {
        return 0;
    }

    uint32_t today = kbo_military_serial_to_yyyymmdd(today_serial);
    if (today == 0u) {
        return 0;
    }

    LONG checked = InterlockedCompareExchange(
        &g_kbo_military_return_preview_last_checked_date,
        (LONG)today,
        (LONG)today);
    if (checked == (LONG)today) {
        return 0;
    }

    uint32_t return_year = today / 10000u;
    KboMilitaryReturnPreviewEntry entries[KBO_MILITARY_RETURN_PREVIEW_MAX];
    memset(entries, 0, sizeof(entries));
    int entry_count = kbo_military_return_preview_collect(
        today_serial,
        return_year,
        entries,
        KBO_MILITARY_RETURN_PREVIEW_MAX);
    if (entry_count <= 0) {
        InterlockedExchange(&g_kbo_military_return_preview_last_checked_date, (LONG)today);
        return 0;
    }

    const KboMilitaryReturnPreviewEntry* lead = &entries[0];
    if (lead->days_left <= 0 || lead->days_left > kbo_military_selection_policy()->return_preview_days) {
        InterlockedExchange(&g_kbo_military_return_preview_last_checked_date, (LONG)today);
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        return 0;
    }

    char marker[128] = {0};
    snprintf(marker, sizeof(marker), "return_preview|%u|%u", return_year, league_id);
    if (kbo_military_return_preview_marker_exists(marker)) {
        InterlockedExchange(&g_kbo_military_return_preview_last_checked_date, (LONG)today);
        return 0;
    }

    char lead_player_name[64] = {0};
    kbo_military_return_preview_player_name(lead, lead_player_name, sizeof(lead_player_name));
    char return_count_text[16] = {0};
    snprintf(return_count_text, sizeof(return_count_text), "%d", entry_count);
    KboNewsTemplateVar title_vars[] = {
        { "lead_player_name", lead_player_name },
        { "return_count", return_count_text },
        { "return_count_plural", entry_count == 1 ? "" : "s" },
    };

    char title[160] = {0};
    char body[8192] = {0};
    if (!kbo_news_template_render_key(
            "military.return_preview.title",
            title_vars,
            (int)(sizeof(title_vars) / sizeof(title_vars[0])),
            title,
            sizeof(title),
            source)
            || !kbo_military_return_preview_render_body(entries, entry_count, body, sizeof(body), source)) {
        append_logf(
            "KBO military return preview news skipped source=%s date=%u reason=template_unavailable count=%d lead=%u",
            source != NULL ? source : "",
            today,
            entry_count,
            lead->player_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        today / 10000u,
        (today / 100u) % 100u,
        today % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    if (created) {
        kbo_military_return_preview_persist_marker(marker, source);
        InterlockedExchange(&g_kbo_military_return_preview_last_checked_date, (LONG)today);
    }
    append_logf(
        "KBO military return preview news source=%s date=%u league_id=%u return_year=%u count=%d lead=%u lead_return=%u days_left=%d created=%d",
        source != NULL ? source : "",
        today,
        league_id,
        return_year,
        entry_count,
        lead->player_id,
        lead->return_yyyymmdd,
        lead->days_left,
        created);
    return created;
}
