#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_name_cache.h"
#include "../../../team/names/team_string.h"
#include "../../players/state/military_player_state.h"
#include "military_selection_news.h"

/* Military service selection news text and link helpers. */

void kbo_military_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

void kbo_military_copy_team_history_name(uint8_t* team, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (team != NULL) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));
        copy_ootp_string_object_text(team, 0x10u, city, sizeof(city));
        if (full_name[0] != '\0') {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0') {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (nickname[0] != '\0') {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
    }
    snprintf(out, out_size, "%s", fallback != NULL ? fallback : "his original club");
}

void kbo_military_copy_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (team_id == 0u) {
        return;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    char team_name[96] = {0};
    kbo_military_copy_team_history_name(team, team_name, sizeof(team_name), NULL);
    if (team_name[0] == '\0' || ascii_equals_ignore_case(team_name, "his original club")) {
        snprintf(team_name, sizeof(team_name), "Team #%u", team_id);
    }
    snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
}

void kbo_military_copy_player_link(uint32_t player_id, uintptr_t player_ptr, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (player_id == 0u) {
        return;
    }

    if (!kbo_player_pointer_plausible(player_ptr)) {
        player_ptr = (uintptr_t)kbo_military_find_player_by_id(player_id);
    }

    char player_name[64] = {0};
    if (kbo_player_pointer_plausible(player_ptr)) {
        kbo_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", player_id);
    }

    snprintf(out, out_size, "<%s:player#%u>", player_name, player_id);
}

int kbo_emit_military_selection_news(
    uint32_t event_yyyymmdd,
    KboMilitarySelectionNewsEntry* entries,
    int entry_count,
    const char* source)
{
    if (event_yyyymmdd == 0u || entries == NULL || entry_count <= 0) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        kbo_log_runtimef(
            "KBO military selection news skipped source=%s date=%u reason=league_id_unavailable count=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            entry_count);
        return 0;
    }

    char title[160] = {0};
    char body[8192] = {0};
    char entry_count_text[16] = {0};
    snprintf(entry_count_text, sizeof(entry_count_text), "%d", entry_count);
    KboNewsTemplateVar summary_vars[] = {
        { "entry_count", entry_count_text },
        { "entry_count_plural", entry_count == 1 ? "" : "s" },
    };
    if (!kbo_news_template_render_key(
            "military.selection.title",
            summary_vars,
            (int)(sizeof(summary_vars) / sizeof(summary_vars[0])),
            title,
            sizeof(title),
            source)
            || !kbo_news_template_render_key(
                "military.selection.intro",
                summary_vars,
                (int)(sizeof(summary_vars) / sizeof(summary_vars[0])),
                body,
                sizeof(body),
                source)) {
        kbo_log_runtimef(
            "KBO military selection news skipped source=%s date=%u reason=template_unavailable count=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            entry_count);
        return 0;
    }
    size_t used = strlen(body);

    char line_template[256] = {0};
    if (!kbo_news_template_load("military.selection.line", line_template, sizeof(line_template), NULL, 0u, source)) {
        kbo_log_runtimef(
            "KBO military selection news skipped source=%s date=%u reason=line_template_unavailable count=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            entry_count);
        return 0;
    }

    for (int i = 0; i < entry_count && used + 180u < sizeof(body); i++) {
        char player_link[96] = {0};
        char team_link[128] = {0};
        char index_text[16] = {0};
        kbo_military_copy_player_link(entries[i].player_id, entries[i].player_ptr, player_link, sizeof(player_link));
        kbo_military_copy_team_link(entries[i].original_team_id, team_link, sizeof(team_link));
        snprintf(index_text, sizeof(index_text), "%d", i + 1);
        KboNewsTemplateVar line_vars[] = {
            { "index", index_text },
            { "player_link", player_link[0] != '\0' ? player_link : "Selected player" },
            { "team_separator", team_link[0] != '\0' ? ", " : "" },
            { "team_link", team_link },
        };
        char rendered_line[256] = {0};
        kbo_news_template_render(
            line_template,
            line_vars,
            (int)(sizeof(line_vars) / sizeof(line_vars[0])),
            rendered_line,
            sizeof(rendered_line));
        kbo_news_text_append(body, sizeof(body), rendered_line);
        used = strlen(body);
    }

    if (used + 220u < sizeof(body)) {
        char outro[384] = {0};
        if (kbo_news_template_render_key("military.selection.outro", NULL, 0, outro, sizeof(outro), source)) {
            kbo_news_text_append(body, sizeof(body), outro);
        }
    }

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    kbo_log_runtimef(
        "KBO military selection news source=%s date=%u league_id=%u count=%d created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        league_id,
        entry_count,
        created);
    return created;
}
