#include "../custom_events_common.h"
#include "emit.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_log.h"
#include "../../core/core_current_date.h"
#include "../../core/core_save_paths.h"
#include "../../core/core_text_date.h"
#include "../../core/core_flags/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../core/core_league_context_parts/league_context_lookup.h"
#include "../../core/core_live_news.h"
#include "../../foreign/foreign_waiver_policy.h"
#include "links.h"

int kbo_emit_asian_games_news(uint32_t event_yyyymmdd, const char* title, const char* lead, const char* source)
{
    if (event_yyyymmdd == 0u || title == NULL || title[0] == '\0') {
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
    kbo_build_asian_games_news_body(body, sizeof(body), title, lead);
    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "KBO Asian Games news source=%s title=%s date=%u league_id=%u created=%d",
        source != NULL ? source : "",
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

    char body[2048] = {0};
    snprintf(
        body,
        sizeof(body),
        "The KBO announced a late change to Korea's Asian Games roster after %s%s%s was ruled unavailable through injury or roster status before the team's departure.\n\n%s%s%s has been added as the replacement, keeping the roster at %d players and preserving the same position group. League officials said the change was made before travel paperwork was finalized, allowing the national team to depart with a full tournament squad.",
        old_player,
        old_team[0] != '\0' ? " of " : "",
        old_team,
        new_player,
        new_team[0] != '\0' ? " of " : "",
        new_team,
        KBO_ASIAN_GAMES_ROSTER_SIZE);

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        "[KBO] Asian Games Roster Change",
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
