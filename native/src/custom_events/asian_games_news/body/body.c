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
#include "../../../runtime_memory/runtime_memory.h"
#include "../links/links.h"

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
    int wrote = snprintf(
        out + *used,
        out_size - *used,
        "%s%s%s%s",
        display_index == 0 ? "" : (display_index == display_count - 1 ? " and " : ", "),
        player_link,
        team_link[0] != '\0' ? " of " : "",
        team_link);
    if (wrote <= 0) {
        return 0;
    }
    *used += (size_t)wrote;
    if (*used < out_size - 1 && bucket[0] != '\0') {
        wrote = snprintf(out + *used, out_size - *used, " (%s)", bucket);
        if (wrote > 0) {
            *used += (size_t)wrote;
        }
    }
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
    if (team_link[0] == '\0') {
        snprintf(team_link, sizeof(team_link), "unattached");
    }

    const char* status = "selected";
    if (entry->returned) {
        status = entry->exempted ? "returned, exempt" : "returned";
    } else if (entry->departed) {
        status = "on tournament leave";
    }

    int wrote = snprintf(
        out + *used,
        out_size - *used,
        "%02ld. %s, %s, %s%s, age %u, %s\n",
        index + 1,
        player_link,
        kbo_asian_games_role_bucket_label(entry->role),
        team_link,
        entry->wildcard ? ", wild card" : "",
        (uint32_t)entry->age,
        status);
    if (wrote <= 0) {
        return 0;
    }
    *used += (size_t)wrote;
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

void kbo_build_asian_games_news_body(char* out, size_t out_size, const char* title, const char* lead)
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
    int include_roster = title != NULL && strstr(title, "Roster") != NULL;
    int include_samples = include_roster;

    int len = snprintf(
        out,
        out_size,
        "%s ",
        lead != NULL && lead[0] != '\0' ? lead : "The KBO issued an Asian Games update.");
    if (len < 0) {
        out[0] = '\0';
        return;
    }

    size_t used = strlen(out);
    if (title != NULL && strstr(title, "Roster") != NULL) {
        len = snprintf(
            out + used,
            out_size - used,
            "The Korean baseball delegation has settled on a %ld-man squad for the Asian Games, balancing young domestic standouts with %d over-age wild-card selection%s. League officials framed the group as a tournament roster built for short-series flexibility rather than a straight all-star list.\n\nThe first wave of attention is expected to fall on ",
            roster_count,
            wildcards,
            wildcards == 1 ? "" : "s");
    } else if (title != NULL && strstr(title, "Depart") != NULL) {
        len = snprintf(
            out + used,
            out_size - used,
            "%d player%s will now leave club duty for the Asian Games window, forcing several teams to cover short-term roster holes during a sensitive part of the league calendar. The delegation will gather before moving into tournament preparation, with the KBO saying clubs have already been notified of the final travel list.",
            departed,
            departed == 1 ? "" : "s");
    } else if (title != NULL && (strstr(title, "Return") != NULL || strstr(title, "Gold") != NULL || strstr(title, "Win") != NULL)) {
        len = snprintf(
            out + used,
            out_size - used,
            "%d player%s returned to their clubs after the Asian Games final, with %d player%s receiving military exemption status as part of Korea's championship result. Teams can now fold the group back into the pennant race and postseason picture.",
            returned,
            returned == 1 ? "" : "s",
            exempted,
            exempted == 1 ? "" : "s");
    } else {
        len = snprintf(
            out + used,
            out_size - used,
            "Korea's Asian Games group stands at %ld players for %u.\n\nThe squad includes ",
            roster_count,
            g_kbo_asian_games_roster_year);
    }
    if (len <= 0) {
        return;
    }
    used += (size_t)len;

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
        len = snprintf(
            out + used,
            out_size - used,
            "%s\n\nThe league said the final decisions reflected player availability, age rules and positional balance, with clubs notified before the roster was made public.",
            include_samples ? "." : "");
        if (len > 0) {
            used += (size_t)len;
        }
    }

    if (include_roster && captain_link[0] != '\0' && used < out_size - 1) {
        len = snprintf(
            out + used,
            out_size - used,
            "\n\n%s, named captain after being selected as one of the wild-card players, said the older players understand what is expected of them. \"This roster belongs to the young players,\" he said. \"Our job is to make the game slow down for them and make sure Korea is ready from the first pitch.\"",
            captain_link);
        if (len > 0) {
            used += (size_t)len;
        }
    }

    if (include_roster && used < out_size - 1) {
        len = snprintf(out + used, out_size - used, "\n\nComplete roster:\n");
        if (len > 0) {
            used += (size_t)len;
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
