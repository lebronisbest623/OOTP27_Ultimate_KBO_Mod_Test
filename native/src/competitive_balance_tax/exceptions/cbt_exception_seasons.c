#include "cbt_exceptions.h"

#include "../../player_team_history/player_team_seasons.h"

int kbo_cbt_exception_player_eligible(uint32_t team_id, const char* player_key, int* out_season_count)
{
    int season_count = 0;
    if (!kbo_player_team_seasons_count_by_key(team_id, player_key, &season_count)) {
        if (out_season_count != NULL) {
            *out_season_count = 0;
        }
        return 0;
    }
    if (out_season_count != NULL) {
        *out_season_count = season_count;
    }
    return season_count >= 7;
}
