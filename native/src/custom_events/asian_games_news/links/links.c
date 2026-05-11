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
        if (full_name[0] != '\0' && strchr(full_name, ' ') != NULL) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0' && !ascii_equals_ignore_case(city, nickname)) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (full_name[0] != '\0') {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (nickname[0] != '\0') {
            snprintf(out, out_size, "%s", nickname);
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

void kbo_copy_asian_games_player_link(KboAsianGamesRosterEntry* entry, char* out, size_t out_size)
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
