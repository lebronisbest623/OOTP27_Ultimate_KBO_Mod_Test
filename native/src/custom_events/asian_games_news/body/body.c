#include "../../runtime/common/custom_events_common.h"
#include "body.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../links/links.h"

static const char* kbo_asian_games_plural(int value)
{
    return value == 1 ? "" : "s";
}

static int kbo_asian_games_news_uses_korean(void)
{
    const char* language_dir = kbo_custom_news_language_dir();
    return language_dir == NULL || strcmp(language_dir, "en") != 0;
}

static void kbo_asian_games_u32_text(uint32_t value, char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        snprintf(out, out_size, "%u", value);
    }
}

static const char* kbo_asian_games_news_role_label(uint8_t role, int use_korean)
{
    const char* bucket = kbo_asian_games_role_bucket_label(role);
    if (!use_korean) {
        return bucket;
    }
    if (strcmp(bucket, "P") == 0) {
        return "\xed\x88\xac\xec\x88\x98";
    }
    if (strcmp(bucket, "C") == 0) {
        return "\xed\x8f\xac\xec\x88\x98";
    }
    if (strcmp(bucket, "IF") == 0) {
        return "\xeb\x82\xb4\xec\x95\xbc\xec\x88\x98";
    }
    if (strcmp(bucket, "OF") == 0) {
        return "\xec\x99\xb8\xec\x95\xbc\xec\x88\x98";
    }
    return "\xec\x95\xbc\xec\x88\x98";
}

static int kbo_asian_games_role_group_index(uint8_t role)
{
    if (kbo_asian_games_role_is_pitcher(role)) {
        return 0;
    }
    if (kbo_asian_games_role_is_catcher(role)) {
        return 1;
    }
    if (kbo_asian_games_role_is_infielder(role)) {
        return 2;
    }
    if (kbo_asian_games_role_is_outfielder(role)) {
        return 3;
    }
    return 4;
}

static const char* kbo_asian_games_role_group_label(int group_index, int use_korean)
{
    if (use_korean) {
        switch (group_index) {
        case 0: return "\xed\x88\xac\xec\x88\x98";
        case 1: return "\xed\x8f\xac\xec\x88\x98";
        case 2: return "\xeb\x82\xb4\xec\x95\xbc\xec\x88\x98";
        case 3: return "\xec\x99\xb8\xec\x95\xbc\xec\x88\x98";
        default: return "\xea\xb8\xb0\xed\x83\x80";
        }
    }
    switch (group_index) {
    case 0: return "Pitchers";
    case 1: return "Catchers";
    case 2: return "Infielders";
    case 3: return "Outfielders";
    default: return "Other";
    }
}

static int kbo_asian_games_append_roster_group_header(
    char* out,
    size_t out_size,
    size_t* used,
    int group_index)
{
    if (out == NULL || out_size == 0 || used == NULL || *used >= out_size - 1) {
        return 0;
    }

    int use_korean = kbo_asian_games_news_uses_korean();
    KboNewsTemplateVar vars[] = {
        { "role_group", kbo_asian_games_role_group_label(group_index, use_korean) },
    };
    char rendered[96] = {0};
    if (!kbo_news_template_render_key(
            "asian_games.roster_group_header",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            "asian_games_body")) {
        return 0;
    }
    kbo_news_text_append(out, out_size, rendered);
    *used = strlen(out);
    return 1;
}

int kbo_asian_games_append_player_blurb(
    char* out,
    size_t out_size,
    size_t* used,
    KboAsianGamesRosterEntry* entry,
    LONG display_index,
    LONG display_count)
{
    if (out == NULL || out_size == 0 || used == NULL || *used >= out_size - 1 || entry == NULL
            || display_index < 0 || display_index >= display_count) {
        return 0;
    }

    char player_link[96] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));

    char team_link[128] = {0};
    kbo_copy_asian_games_team_link(entry->original_team_id, team_link, sizeof(team_link));

    int use_korean = kbo_asian_games_news_uses_korean();
    const char* bucket = kbo_asian_games_news_role_label(entry->role, use_korean);
    char role_suffix[64] = {0};
    if (bucket[0] != '\0') {
        snprintf(role_suffix, sizeof(role_suffix), " (%s)", bucket);
    }
    const char* separator = "";
    if (display_index != 0) {
        separator = use_korean ? ", " : (display_index == display_count - 1 ? " and " : ", ");
    }
    const char* team_prefix = "";
    if (team_link[0] != '\0') {
        team_prefix = use_korean ? " / " : " of ";
    }

    KboNewsTemplateVar vars[] = {
        { "separator", separator },
        { "player_link", player_link },
        { "team_prefix", team_prefix },
        { "team_link", team_link },
        { "role_suffix", role_suffix },
    };
    char rendered[256] = {0};
    if (!kbo_news_template_render_key(
            "asian_games.player_blurb",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            "asian_games_body")) {
        return 0;
    }
    kbo_news_text_append(out, out_size, rendered);
    *used = strlen(out);
    return 1;
}

int kbo_asian_games_append_roster_line(
    char* out,
    size_t out_size,
    size_t* used,
    LONG index,
    KboAsianGamesRosterEntry* entry)
{
    if (out == NULL || out_size == 0 || used == NULL || *used >= out_size - 1 || entry == NULL) {
        return 0;
    }

    char player_link[96] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));

    char team_link[128] = {0};
    kbo_copy_asian_games_team_link(entry->original_team_id, team_link, sizeof(team_link));
    int use_korean = kbo_asian_games_news_uses_korean();
    if (team_link[0] == '\0') {
        snprintf(
            team_link,
            sizeof(team_link),
            "%s",
            use_korean ? "\xec\x86\x8c\xec\x86\x8d \xec\x97\x86\xec\x9d\x8c" : "unattached");
    }

    const char* status = use_korean ? "\xec\x84\xa0\xeb\xb0\x9c" : "selected";
    if (entry->returned) {
        status = use_korean
            ? (entry->exempted
                ? "\xeb\xb3\xb5\xea\xb7\x80, \xeb\xb3\x91\xec\x97\xad \xed\x98\x9c\xed\x83\x9d"
                : "\xeb\xb3\xb5\xea\xb7\x80, \xed\x98\x9c\xed\x83\x9d \xec\x97\x86\xec\x9d\x8c")
            : (entry->exempted ? "returned, exempt" : "returned, no exemption");
    } else if (entry->departed) {
        status = use_korean ? "\xec\xb0\xa8\xec\xb6\x9c \xec\xa4\x91" : "on tournament leave";
    }

    char index_text[16] = {0};
    char age_text[16] = {0};
    snprintf(index_text, sizeof(index_text), "%02ld", index + 1);
    snprintf(age_text, sizeof(age_text), "%u", (uint32_t)entry->age);
    KboNewsTemplateVar vars[] = {
        { "index", index_text },
        { "player_link", player_link },
        { "role_bucket", kbo_asian_games_news_role_label(entry->role, use_korean) },
        { "team_link", team_link },
        { "wildcard_text", entry->wildcard
            ? (use_korean ? ", \xec\x99\x80\xec\x9d\xbc\xeb\x93\x9c\xec\xb9\xb4\xeb\x93\x9c" : ", wild card")
            : "" },
        { "age", age_text },
        { "status", status },
    };
    char rendered[256] = {0};
    if (!kbo_news_template_render_key(
            "asian_games.roster_line",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            "asian_games_body")) {
        return 0;
    }
    kbo_news_text_append(out, out_size, rendered);
    *used = strlen(out);
    return 1;
}

KboAsianGamesRosterEntry* kbo_asian_games_choose_captain(void)
{
    KboAsianGamesRosterEntry* best = NULL;
    for (LONG i = 0; i < g_kbo_asian_games_roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->wildcard == 0u) {
            continue;
        }
        if (best == NULL || entry->score > best->score) {
            best = entry;
        }
    }
    if (best != NULL) {
        return best;
    }
    for (LONG i = 0; i < g_kbo_asian_games_roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id != 0u && (best == NULL || entry->score > best->score)) {
            best = entry;
        }
    }
    return best;
}

#include "asian_games_news_body_build.inc"
