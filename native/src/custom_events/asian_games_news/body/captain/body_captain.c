#include "../../../runtime/common/custom_events_common.h"
#include "../body.h"

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
