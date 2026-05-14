

#include "asian_games_state.h"

KboAsianGamesRosterEntry g_kbo_asian_games_roster[KBO_ASIAN_GAMES_ROSTER_SIZE];
LONG g_kbo_asian_games_roster_count = 0;
uint32_t g_kbo_asian_games_roster_year = 0;
uint8_t g_kbo_asian_games_result = KBO_ASIAN_GAMES_RESULT_UNKNOWN;
char g_kbo_asian_games_roster_save_path[MAX_PATH] = {0};
