#ifndef KBOFIX_SRC_CAPTAIN_API_CAPTAIN_SELECTION_H_
#define KBOFIX_SRC_CAPTAIN_API_CAPTAIN_SELECTION_H_

#include <stdint.h>
#include <stddef.h>

int kbo_run_captain_preseason_selection_once(const char* source);
void start_kbo_captain_preseason_selection_thread(void);
int kbo_get_captain_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    char* out_name,
    size_t out_name_size,
    uint32_t* out_player_id,
    char* out_source,
    size_t out_source_size);
int kbo_captain_player_is_team_captain(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint32_t player_id);

#endif
