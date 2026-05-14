#include "../../runtime/common/custom_events_common.h"
#include "emit.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../links/links.h"

static int kbo_asian_games_news_uses_korean(void)
{
    const char* language_dir = kbo_custom_news_language_dir();
    return language_dir == NULL || strcmp(language_dir, "en") != 0;
}

int kbo_emit_asian_games_news(uint32_t event_yyyymmdd, const char* template_prefix, const char* source)
{
    if (event_yyyymmdd == 0u || template_prefix == NULL || template_prefix[0] == '\0') {
        return 0;
    }

    char title_key[128] = {0};
    char lead_key[128] = {0};
    char title[160] = {0};
    char lead[256] = {0};
    snprintf(title_key, sizeof(title_key), "%s.title", template_prefix);
    snprintf(lead_key, sizeof(lead_key), "%s.lead", template_prefix);
    if (!kbo_news_template_render_key(title_key, NULL, 0, title, sizeof(title), source)
            || !kbo_news_template_render_key(lead_key, NULL, 0, lead, sizeof(lead), source)) {
        append_logf(
            "KBO Asian Games news skipped source=%s template=%s reason=template_unavailable",
            source != NULL ? source : "",
            template_prefix);
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        append_logf(
            "KBO Asian Games news skipped source=%s title=%s reason=league_id_unavailable",
            source != NULL ? source : "",
            title);
        return 0;
    }

    char body[8192] = {0};
    kbo_build_asian_games_news_body(body, sizeof(body), template_prefix, lead, source);
    if (body[0] == '\0') {
        append_logf(
            "KBO Asian Games news skipped source=%s template=%s title=%s reason=body_template_unavailable",
            source != NULL ? source : "",
            template_prefix,
            title);
        return 0;
    }
    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "KBO Asian Games news source=%s template=%s title=%s date=%u league_id=%u created=%d",
        source != NULL ? source : "",
        template_prefix,
        title,
        event_yyyymmdd,
        league_id,
        created);
    return created;
}

uint32_t kbo_asian_games_effective_action_date(uint32_t event_yyyymmdd)
{
    uint32_t current_year = 0u;
    uint32_t current_month = 0u;
    uint32_t current_day = 0u;
    if (!kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        return event_yyyymmdd;
    }

    uint32_t current_yyyymmdd = current_year * 10000u + current_month * 100u + current_day;
    if (event_yyyymmdd == 0u || current_yyyymmdd > event_yyyymmdd) {
        return current_yyyymmdd;
    }
    return event_yyyymmdd;
}

int kbo_emit_asian_games_replacement_news(
    uint32_t event_yyyymmdd,
    KboAsianGamesRosterEntry* old_entry,
    KboAsianGamesRosterEntry* new_entry,
    const char* source)
{
    if (event_yyyymmdd == 0u || old_entry == NULL || new_entry == NULL) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        return 0;
    }

    char old_player[96] = {0};
    char new_player[96] = {0};
    char old_team[128] = {0};
    char new_team[128] = {0};
    kbo_copy_asian_games_player_link(old_entry, old_player, sizeof(old_player));
    kbo_copy_asian_games_player_link(new_entry, new_player, sizeof(new_player));
    kbo_copy_asian_games_team_link(old_entry->original_team_id, old_team, sizeof(old_team));
    kbo_copy_asian_games_team_link(new_entry->original_team_id, new_team, sizeof(new_team));

    int use_korean = kbo_asian_games_news_uses_korean();
    const char* team_prefix = use_korean ? " / " : " of ";
    char body[2048] = {0};
    char title[160] = {0};
    char roster_size_text[16] = {0};
    snprintf(roster_size_text, sizeof(roster_size_text), "%d", kbo_asian_games_policy_roster_size());
    KboNewsTemplateVar vars[] = {
        { "old_player", old_player },
        { "old_team_prefix", old_team[0] != '\0' ? team_prefix : "" },
        { "old_team", old_team },
        { "new_player", new_player },
        { "new_team_prefix", new_team[0] != '\0' ? team_prefix : "" },
        { "new_team", new_team },
        { "roster_size", roster_size_text },
    };
    if (!kbo_news_template_render_key(
            "asian_games.replacement.title",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            source)
            || !kbo_news_template_render_key(
                "asian_games.replacement.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        append_logf(
            "KBO Asian Games replacement news skipped source=%s date=%u old_player=%u new_player=%u reason=template_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd,
            old_entry->player_id,
            new_entry->player_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "KBO Asian Games replacement news source=%s date=%u old_player=%u new_player=%u created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        old_entry->player_id,
        new_entry->player_id,
        created);
    return created;
}
