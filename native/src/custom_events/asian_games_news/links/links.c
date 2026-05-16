#include "../../runtime/common/custom_events_common.h"
#include "links.h"
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
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_name_cache.h"
#include "../../../team/names/team_string.h"

static char kbo_asian_games_ascii_lower(char ch)
{
    return ch >= 'A' && ch <= 'Z' ? (char)(ch - 'A' + 'a') : ch;
}

static int kbo_asian_games_ascii_equal_ignore_case(const char* a, const char* b, size_t len)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    for (size_t i = 0u; i < len; i++) {
        if (kbo_asian_games_ascii_lower(a[i]) != kbo_asian_games_ascii_lower(b[i])) {
            return 0;
        }
    }
    return 1;
}

static int kbo_asian_games_should_strip_team_suffix(
    const char* text,
    size_t first_len,
    const char* suffix,
    size_t suffix_len)
{
    if (text == NULL || suffix == NULL || first_len == 0u || suffix_len < 2u || suffix_len > 4u) {
        return 0;
    }
    if (first_len == suffix_len && kbo_asian_games_ascii_equal_ignore_case(text, suffix, suffix_len)) {
        return 1;
    }
    if (first_len >= suffix_len && kbo_asian_games_ascii_equal_ignore_case(text, suffix, suffix_len)) {
        return 1;
    }
    char first = kbo_asian_games_ascii_lower(text[0]);
    for (size_t i = 0u; i < suffix_len; i++) {
        if (kbo_asian_games_ascii_lower(suffix[i]) != first) {
            return 0;
        }
    }
    return 1;
}

static void kbo_copy_asian_games_display_team_name(const char* raw, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (raw == NULL || raw[0] == '\0') {
        return;
    }

    size_t len = strlen(raw);
    while (len > 0u && (raw[len - 1u] == ' ' || raw[len - 1u] == '\t')) {
        len--;
    }

    size_t split = len;
    while (split > 0u && raw[split - 1u] != ' ' && raw[split - 1u] != '\t') {
        split--;
    }
    if (split > 0u) {
        size_t first_len = 0u;
        while (first_len < len && raw[first_len] != ' ' && raw[first_len] != '\t') {
            first_len++;
        }

        size_t suffix_start = split;
        while (suffix_start < len && (raw[suffix_start] == ' ' || raw[suffix_start] == '\t')) {
            suffix_start++;
        }
        if (suffix_start < len
                && kbo_asian_games_should_strip_team_suffix(raw, first_len, raw + suffix_start, len - suffix_start)) {
            len = split;
            while (len > 0u && (raw[len - 1u] == ' ' || raw[len - 1u] == '\t')) {
                len--;
            }
        }
    }

    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, raw, len);
    out[len] = '\0';
}

void kbo_copy_asian_games_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, 0x10u, city, sizeof(city));
        copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));
        if (full_name[0] != '\0'
                && (strchr(full_name, ' ') != NULL || kbo_ootp_text_has_non_ascii(full_name))) {
            kbo_copy_asian_games_display_team_name(full_name, out, out_size);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0' && !ascii_equals_ignore_case(city, nickname)) {
            char combined[128] = {0};
            snprintf(combined, sizeof(combined), "%s %s", city, nickname);
            kbo_copy_asian_games_display_team_name(combined, out, out_size);
            return;
        }
        if (full_name[0] != '\0') {
            kbo_copy_asian_games_display_team_name(full_name, out, out_size);
            return;
        }
        if (nickname[0] != '\0') {
            kbo_copy_asian_games_display_team_name(nickname, out, out_size);
            return;
        }
    }

    if (team_id != 0u) {
        snprintf(out, out_size, "his club");
    }
}

void kbo_copy_asian_games_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    kbo_copy_asian_games_team_name(team_id, team_name, sizeof(team_name));
    if (team_name[0] == '\0' || ascii_equals_ignore_case(team_name, "his club")) {
        if (team_id != 0u) {
            snprintf(out, out_size, "<Team #%u:team#%u>", team_id, team_id);
        }
        return;
    }
    if (team_id != 0u) {
        snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
    } else {
        snprintf(out, out_size, "%s", team_name);
    }
}

void kbo_copy_asian_games_player_link(const KboAsianGamesRosterEntry* entry, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (entry == NULL || entry->player_id == 0u) {
        snprintf(out, out_size, "a Korea squad member");
        return;
    }

    uintptr_t player_ptr = entry->player_ptr;
    if (!kbo_player_pointer_plausible(player_ptr)) {
        player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
    }

    char player_name[64] = {0};
    if (kbo_player_pointer_plausible(player_ptr)) {
        kbo_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", entry->player_id);
    }

    snprintf(out, out_size, "<%s:player#%u>", player_name, entry->player_id);
}
