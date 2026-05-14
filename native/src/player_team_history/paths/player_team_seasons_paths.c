#include "../internal/player_team_seasons_internal.h"

static int kbo_player_team_seasons_global_seed_path(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_global_data_file(file_name, out, out_size);
}

static int kbo_player_team_seasons_bundled_seed_path(const char* relative_path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u || relative_path == NULL || relative_path[0] == '\0') {
        return 0;
    }
    out[0] = '\0';

    HMODULE module = GetModuleHandleA("KBOFix.dll");
    if (module == NULL) {
        module = GetModuleHandleA(NULL);
    }

    char path[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameA(module, path, (DWORD)sizeof(path));
    if (len == 0u || len >= (DWORD)sizeof(path)) {
        return 0;
    }
    char* slash = strrchr(path, '\\');
    if (slash == NULL) {
        return 0;
    }
    *(slash + 1) = '\0';

    int written = snprintf(out, out_size, "%sdata\\seeds\\%s", path, relative_path);
    return written > 0 && written < (int)out_size;
}

KboCsvReader* kbo_player_team_seasons_open_seed(char* out_path, size_t out_path_size)
{
    static const char* global_names[] = {
        "player_team_seasons_seed.csv",
        "cbt_player_team_seasons_seed.csv"
    };
    static const char* bundled_paths[] = {
        "player_team_history\\player_team_seasons_seed.csv",
        "player_team_seasons_seed.csv",
        "competitive_balance_tax\\cbt_player_team_seasons_seed.csv",
        "cbt_player_team_seasons_seed.csv"
    };

    char path[MAX_PATH] = {0};
    for (int i = 0; i < (int)(sizeof(global_names) / sizeof(global_names[0])); i++) {
        if (!kbo_player_team_seasons_global_seed_path(global_names[i], path, sizeof(path))) {
            continue;
        }
        KboCsvReader* reader = kbo_csv_reader_open(path);
        if (reader != NULL) {
            if (out_path != NULL && out_path_size > 0u) {
                snprintf(out_path, out_path_size, "%s", path);
            }
            return reader;
        }
    }

    for (int i = 0; i < (int)(sizeof(bundled_paths) / sizeof(bundled_paths[0])); i++) {
        if (!kbo_player_team_seasons_bundled_seed_path(bundled_paths[i], path, sizeof(path))) {
            continue;
        }
        KboCsvReader* reader = kbo_csv_reader_open(path);
        if (reader != NULL) {
            if (out_path != NULL && out_path_size > 0u) {
                snprintf(out_path, out_path_size, "%s", path);
            }
            return reader;
        }
    }

    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    return NULL;
}
