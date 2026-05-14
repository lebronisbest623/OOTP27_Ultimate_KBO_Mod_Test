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

    const char* bucket = kbo_asian_games_role_bucket_label(entry->role);
    char role_suffix[64] = {0};
    if (bucket[0] != '\0') {
        snprintf(role_suffix, sizeof(role_suffix), " (%s)", bucket);
    }
    int use_korean = kbo_asian_games_news_uses_korean();
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
        { "role_bucket", kbo_asian_games_role_bucket_label(entry->role) },
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

void kbo_build_asian_games_news_body(char* out, size_t out_size, const char* template_prefix, const char* lead, const char* source)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    int departed = 0;
    int returned = 0;
    int exempted = 0;
    int wildcards = 0;
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].departed) { departed++; }
        if (g_kbo_asian_games_roster[i].returned) { returned++; }
        if (g_kbo_asian_games_roster[i].exempted) { exempted++; }
        if (g_kbo_asian_games_roster[i].wildcard) { wildcards++; }
    }
    KboAsianGamesRosterEntry* captain = kbo_asian_games_choose_captain();
    char captain_link[96] = {0};
    if (captain != NULL) {
        kbo_copy_asian_games_player_link(captain, captain_link, sizeof(captain_link));
    }
    int is_selection = template_prefix != NULL && strcmp(template_prefix, "asian_games.selection") == 0;
    int is_departure = template_prefix != NULL && strcmp(template_prefix, "asian_games.departure") == 0;
    int is_final_gold = template_prefix != NULL && strcmp(template_prefix, "asian_games.final") == 0;
    int is_final_failure = template_prefix != NULL && strcmp(template_prefix, "asian_games.final.failure") == 0;
    int is_final = is_final_gold || is_final_failure;
    int include_roster = is_selection;
    int include_samples = include_roster;

    kbo_news_text_append(out, out_size, lead != NULL && lead[0] != '\0' ? lead : "");
    if (out[0] != '\0') {
        kbo_news_text_append(out, out_size, " ");
    }

    size_t used = strlen(out);
    (void)used;

    char roster_count_text[16] = {0};
    char wildcards_text[16] = {0};
    char departed_text[16] = {0};
    char returned_text[16] = {0};
    char exempted_text[16] = {0};
    char roster_year_text[16] = {0};
    kbo_asian_games_u32_text((uint32_t)roster_count, roster_count_text, sizeof(roster_count_text));
    kbo_asian_games_u32_text((uint32_t)wildcards, wildcards_text, sizeof(wildcards_text));
    kbo_asian_games_u32_text((uint32_t)departed, departed_text, sizeof(departed_text));
    kbo_asian_games_u32_text((uint32_t)returned, returned_text, sizeof(returned_text));
    kbo_asian_games_u32_text((uint32_t)exempted, exempted_text, sizeof(exempted_text));
    kbo_asian_games_u32_text(g_kbo_asian_games_roster_year, roster_year_text, sizeof(roster_year_text));

    KboNewsTemplateVar body_vars[] = {
        { "roster_count", roster_count_text },
        { "wildcards", wildcards_text },
        { "wildcards_plural", kbo_asian_games_plural(wildcards) },
        { "departed", departed_text },
        { "departed_plural", kbo_asian_games_plural(departed) },
        { "returned", returned_text },
        { "returned_plural", kbo_asian_games_plural(returned) },
        { "exempted", exempted_text },
        { "exempted_plural", kbo_asian_games_plural(exempted) },
        { "roster_year", roster_year_text },
    };
    const char* body_key = is_selection
        ? "asian_games.selection.body"
        : (is_departure
            ? "asian_games.departure.body"
            : (is_final_failure
                ? "asian_games.final.failure.body"
                : (is_final ? "asian_games.final.body" : "asian_games.default.body")));
    char rendered[2048] = {0};
    if (!kbo_news_template_render_key(
            body_key,
            body_vars,
            (int)(sizeof(body_vars) / sizeof(body_vars[0])),
            rendered,
            sizeof(rendered),
            source)) {
        out[0] = '\0';
        return;
    }
    kbo_news_text_append(out, out_size, rendered);
    used = strlen(out);

    if (include_samples) {
        LONG sample_count = roster_count < 6 ? roster_count : 6;
        for (LONG i = 0; i < sample_count && used + 160u < out_size; i++) {
            LONG roster_index = (i == sample_count - 1 && roster_count > sample_count)
                ? roster_count - 1
                : i;
            kbo_asian_games_append_player_blurb(
                out,
                out_size,
                &used,
                &g_kbo_asian_games_roster[roster_index],
                i,
                sample_count);
        }
    }

    if (used < out_size - 1) {
        KboNewsTemplateVar suffix_vars[] = {
            { "period", include_samples ? "." : "" },
        };
        char suffix[512] = {0};
        if (kbo_news_template_render_key(
                "asian_games.samples_suffix",
                suffix_vars,
                (int)(sizeof(suffix_vars) / sizeof(suffix_vars[0])),
                suffix,
                sizeof(suffix),
                source)) {
            kbo_news_text_append(out, out_size, suffix);
            used = strlen(out);
        }
    }

    if (include_roster && captain_link[0] != '\0' && used < out_size - 1) {
        KboNewsTemplateVar quote_vars[] = {
            { "captain_link", captain_link },
        };
        char quote[768] = {0};
        if (kbo_news_template_render_key(
                "asian_games.captain_quote",
                quote_vars,
                (int)(sizeof(quote_vars) / sizeof(quote_vars[0])),
                quote,
                sizeof(quote),
                source)) {
            kbo_news_text_append(out, out_size, quote);
            used = strlen(out);
        }
    }

    if (include_roster && used < out_size - 1) {
        char header[128] = {0};
        if (kbo_news_template_load("asian_games.complete_roster_header", header, sizeof(header), NULL, 0u, source)) {
            kbo_news_text_append(out, out_size, header);
            used = strlen(out);
        }
    }

    if (include_roster) {
        for (LONG i = 0; i < roster_count && used + 140u < out_size; i++) {
            kbo_asian_games_append_roster_line(
                out,
                out_size,
                &used,
                i,
                &g_kbo_asian_games_roster[i]);
        }
    }
}
