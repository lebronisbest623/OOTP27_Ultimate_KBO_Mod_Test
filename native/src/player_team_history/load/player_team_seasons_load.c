#include "../internal/player_team_seasons_internal.h"

void kbo_player_team_seasons_ensure_seed_loaded(void)
{
    if (InterlockedCompareExchange(&g_kbo_player_team_season_seed_loaded, 1, 1) == 1) {
        return;
    }

    kbo_player_team_seasons_lock();
    if (g_kbo_player_team_season_seed_loaded) {
        kbo_player_team_seasons_unlock();
        return;
    }

    char path[MAX_PATH] = {0};
    KboCsvReader* reader = kbo_player_team_seasons_open_seed(path, sizeof(path));
    if (reader == NULL) {
        append_log_line("KBO player-team season seed unavailable");
        InterlockedExchange(&g_kbo_player_team_season_seed_loaded, 1);
        kbo_player_team_seasons_unlock();
        return;
    }

    while (g_kbo_player_team_season_seed_count < KBO_PLAYER_TEAM_SEASON_SEED_MAX
            && kbo_csv_reader_next_row(reader)) {
        char fields[5][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 5);
        if (field_count < 5
                || fields[0][0] == '\0'
                || fields[0][0] == '#'
                || _stricmp(fields[0], "player_key") == 0) {
            continue;
        }

        KboPlayerTeamSeasonRow row;
        memset(&row, 0, sizeof(row));
        snprintf(row.player_key, sizeof(row.player_key), "%.*s", (int)sizeof(row.player_key) - 1, fields[0]);
        snprintf(row.team_code, sizeof(row.team_code), "%.*s", (int)sizeof(row.team_code) - 1, fields[3]);
        row.season_count = (int)strtol(fields[4], NULL, 10);
        if (row.player_key[0] != '\0' && row.team_code[0] != '\0') {
            g_kbo_player_team_season_seed[g_kbo_player_team_season_seed_count++] = row;
        }
    }

    kbo_csv_reader_close(reader);
    append_logf(
        "KBO player-team season seed loaded rows=%d path=%s",
        g_kbo_player_team_season_seed_count,
        path);
    InterlockedExchange(&g_kbo_player_team_season_seed_loaded, 1);
    kbo_player_team_seasons_unlock();
}
