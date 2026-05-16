#include "../internal/player_team_seasons_internal.h"

KboPlayerTeamSeasonRow g_kbo_player_team_season_seed[KBO_PLAYER_TEAM_SEASON_SEED_MAX];
int g_kbo_player_team_season_seed_count = 0;
volatile LONG g_kbo_player_team_season_seed_loaded = 0;
KboSpinLock g_kbo_player_team_season_seed_lock = KBO_SPIN_LOCK_INIT;

void kbo_player_team_seasons_lock(void)
{
    kbo_spin_lock(&g_kbo_player_team_season_seed_lock);
}

void kbo_player_team_seasons_unlock(void)
{
    kbo_spin_unlock(&g_kbo_player_team_season_seed_lock);
}
